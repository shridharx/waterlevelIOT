'use strict';

/**
 * Smart IoT Water Tank Monitoring System - backend
 * -------------------------------------------------
 * Express + Socket.IO server that
 *   1. accepts authenticated readings from the ESP32,
 *   2. keeps the latest reading in memory (no database),
 *   3. broadcasts every reading to the dashboard over Socket.IO,
 *   4. serves the dashboard as static files.
 *
 * Endpoints
 *   POST /api/water-level   ESP32 -> server (requires x-api-key)
 *   GET  /api/water-level   latest reading
 *   GET  /api/status        device + server status
 *   GET  /api/health        liveness probe for Render
 */

require('dotenv').config();

const path = require('node:path');
const http = require('node:http');
const crypto = require('node:crypto');
const express = require('express');
const cors = require('cors');
const { Server } = require('socket.io');
const alerts = require('./lib/alerts');

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
const PORT = Number(process.env.PORT) || 3000;
const DEVICE_API_KEY = (process.env.DEVICE_API_KEY || '').trim();
const DEVICE_ID = (process.env.DEVICE_ID || '').trim(); // optional expected device
const OFFLINE_AFTER_MS = Number(process.env.OFFLINE_AFTER_MS) || 15000;
const CORS_ORIGINS = (process.env.CORS_ORIGIN || '*')
  .split(',')
  .map((o) => o.trim())
  .filter(Boolean);

const MAX_FIELD_CM = 500;
const SERVER_VERSION = '1.0.0';
const startedAt = Date.now();

if (!DEVICE_API_KEY) {
  console.error(
    '[FATAL] DEVICE_API_KEY is not set. Copy backend/.env.example to backend/.env\n' +
      '        and set a long random key, then also put the same key in the firmware.'
  );
  process.exit(1);
}

// ---------------------------------------------------------------------------
// In-memory state (intentionally no database)
// ---------------------------------------------------------------------------
const state = {
  latest: null,        // last accepted reading
  deviceOnline: false,
  lastSeenMs: 0,
  lastSeenAt: null,
  firstSeenAt: null,
  readings: 0,
  sensorOk: true,
  lastSensorErrorAt: null,
};

const app = express();
app.disable('x-powered-by');
app.set('trust proxy', 1); // Render terminates TLS in front of the app

const httpServer = http.createServer(app);
const io = new Server(httpServer, {
  cors: { origin: CORS_ORIGINS.includes('*') ? '*' : CORS_ORIGINS, methods: ['GET'] },
});

app.use(
  cors({
    origin: CORS_ORIGINS.includes('*') ? '*' : CORS_ORIGINS,
    methods: ['GET', 'POST'],
    allowedHeaders: ['Content-Type', 'x-api-key', 'Authorization'],
  })
);
app.use(express.json({ limit: '8kb' }));

// Minimal hardening headers (works over Render's HTTPS and locally).
app.use((req, res, next) => {
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('Referrer-Policy', 'no-referrer');
  next();
});

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
const nowIso = () => new Date().toISOString();

const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));

const round1 = (v) => Math.round(v * 10) / 10;

function safeEqual(a, b) {
  const bufA = Buffer.from(String(a));
  const bufB = Buffer.from(String(b));
  if (bufA.length !== bufB.length) return false;
  return crypto.timingSafeEqual(bufA, bufB);
}

function requestKey(req) {
  const headerKey = req.get('x-api-key');
  if (headerKey) return headerKey;
  const auth = req.get('authorization') || '';
  if (auth.toLowerCase().startsWith('bearer ')) return auth.slice(7).trim();
  return '';
}

function isFiniteNumber(v) {
  return typeof v === 'number' && Number.isFinite(v);
}

function round(value, decimals) {
  const f = 10 ** decimals;
  return Math.round(value * f) / f;
}

/**
 * Validates and normalises an incoming reading.
 * Returns { ok: true, value } or { ok: false, error }.
 */
function validateReading(body) {
  if (!body || typeof body !== 'object' || Array.isArray(body)) {
    return { ok: false, error: 'Body must be a JSON object' };
  }

  const deviceId = typeof body.deviceId === 'string' ? body.deviceId.trim() : '';
  if (!/^[A-Za-z0-9._-]{1,64}$/.test(deviceId)) {
    return { ok: false, error: 'deviceId must match [A-Za-z0-9._-]{1,64}' };
  }
  if (DEVICE_ID && deviceId !== DEVICE_ID) {
    return { ok: false, error: `Unknown deviceId "${deviceId}"` };
  }

  const valid = body.valid !== false;
  const tankHeightCm = isFiniteNumber(body.tankHeightCm)
    ? body.tankHeightCm
    : isFiniteNumber(body.emptyDistanceCm)
      ? body.emptyDistanceCm
      : null;

  if (tankHeightCm !== null && (tankHeightCm <= 0 || tankHeightCm > MAX_FIELD_CM)) {
    return { ok: false, error: `tankHeightCm must be between 0 and ${MAX_FIELD_CM}` };
  }

  let distanceCm = isFiniteNumber(body.distanceCm) ? body.distanceCm : null;

  if (!valid) {
    // A sensor error still proves the device is alive, but carries no level.
    return {
      ok: true,
      value: { deviceId, valid: false, distanceCm: null, waterHeightCm: null, percent: null, tankHeightCm, emptyDistanceCm: tankHeightCm },
    };
  }

  if (distanceCm === null || distanceCm < 0 || distanceCm > MAX_FIELD_CM) {
    return { ok: false, error: `distanceCm must be a number between 0 and ${MAX_FIELD_CM}` };
  }
  if (tankHeightCm === null) {
    return { ok: false, error: 'tankHeightCm is required' };
  }

  const emptyDistanceCm = isFiniteNumber(body.emptyDistanceCm)
    ? body.emptyDistanceCm
    : tankHeightCm;

  if (emptyDistanceCm <= 0 || emptyDistanceCm > MAX_FIELD_CM) {
    return { ok: false, error: `emptyDistanceCm must be between 0 and ${MAX_FIELD_CM}` };
  }

  // The backend recomputes the level so a firmware bug can never publish an
  // impossible percentage. Formula matches the report:
  //   water height = empty distance - measured distance
  //   percentage   = water height / tank height * 100
  const waterHeightCm = clamp(emptyDistanceCm - distanceCm, 0, tankHeightCm);
  const percent = clamp((waterHeightCm / tankHeightCm) * 100, 0, 100);

  const value = {
    deviceId,
    valid: true,
    distanceCm: round1(distanceCm),
    waterHeightCm: round1(waterHeightCm),
    percent: round(percent, 1),
    tankHeightCm: round1(tankHeightCm),
    emptyDistanceCm: round1(emptyDistanceCm),
    samples: isFiniteNumber(body.samples) ? Math.max(0, Math.round(body.samples)) : null,
    rssi: isFiniteNumber(body.rssi) ? Math.round(body.rssi) : null,
    uptimeSec: isFiniteNumber(body.uptimeSec) ? Math.max(0, Math.round(body.uptimeSec)) : null,
    firmware: typeof body.firmware === 'string' ? body.firmware.slice(0, 32) : null,
  };

  return { ok: true, value };
}

function deviceStatus() {
  return {
    online: state.deviceOnline,
    deviceId: state.latest ? state.latest.deviceId : null,
    lastSeenAt: state.lastSeenAt,
    lastSensorErrorAt: state.lastSensorErrorAt,
    sensorOk: state.sensorOk,
    readings: state.readings,
    offlineAfterMs: OFFLINE_AFTER_MS,
  };
}

function emitSnapshot(socket) {
  socket.emit('meta', {
    serverVersion: SERVER_VERSION,
    offlineAfterMs: OFFLINE_AFTER_MS,
    serverTime: nowIso(),
  });
  socket.emit('device:status', deviceStatus());
  if (state.latest) socket.emit('water:update', state.latest);
}

// ---------------------------------------------------------------------------
// ESP32 ingestion
// ---------------------------------------------------------------------------
function ingest(req, res) {
  if (!safeEqual(requestKey(req), DEVICE_API_KEY)) {
    return res.status(401).json({ ok: false, error: 'Invalid or missing device API key' });
  }

  const check = validateReading(req.body);
  if (!check.ok) {
    return res.status(400).json({ ok: false, error: check.error });
  }

  const value = check.value;
  const now = Date.now();
  const wasOnline = state.deviceOnline;

  state.deviceOnline = true;
  state.lastSeenMs = now;
  state.lastSeenAt = nowIso();
  state.firstSeenAt = state.firstSeenAt || now;

  if (wasOnline === false) {
    io.emit('device:status', deviceStatus());
    console.log(`[device] ${value.deviceId} ONLINE`);
  }

  if (!value.valid) {
    state.sensorOk = false;
    state.lastSensorErrorAt = state.lastSeenAt;
    io.emit('device:status', deviceStatus());
    io.emit('sensor:error', {
      deviceId: value.deviceId,
      at: state.lastSeenAt,
      message: 'ESP32 reported no valid ultrasonic echo',
    });
    console.warn('[sensor] invalid reading from', value.deviceId);
    return res.status(202).json({
      ok: true,
      accepted: false,
      reason: 'sensor-error',
      receivedAt: state.lastSeenAt,
    });
  }

  state.latest = { ...value, receivedAt: state.lastSeenAt };
  state.readings += 1;
  state.sensorOk = true;

  io.emit('water:update', state.latest);
  io.emit('device:status', deviceStatus());

  // LOW / FULL notifications run in the background; never block or fail the
  // ingestion response because a notification service is unreachable.
  alerts
    .evaluateReading(state.latest)
    .catch((err) => console.error('[alerts] unexpected:', err.message));

  console.log(
    `[water] ${value.percent.toFixed(1)}%  ${value.waterHeightCm}cm / ${value.tankHeightCm}cm` +
      ` (distance ${value.distanceCm}cm, ${value.samples ?? '?'} samples)`
  );

  return res.status(200).json({
    ok: true,
    accepted: true,
    receivedAt: state.latest.receivedAt,
    percent: value.percent,
  });
}

app.post('/api/water-level', ingest);

// ---------------------------------------------------------------------------
// Read APIs
// ---------------------------------------------------------------------------
app.get('/api/water-level', (req, res) => {
  if (!state.latest) {
    return res.status(200).json({
      ok: true,
      data: null,
      message: 'No reading received yet',
      device: deviceStatus(),
    });
  }
  res.setHeader('Cache-Control', 'no-store');
  res.json({ ok: true, data: state.latest, device: deviceStatus() });
});

app.get('/api/status', (req, res) => {
  res.setHeader('Cache-Control', 'no-store');
  res.json({
    ok: true,
    server: {
      version: SERVER_VERSION,
      uptimeSec: Math.round((Date.now() - startedAt) / 1000),
      now: nowIso(),
      sockets: io.engine.clientsCount,
      database: false,
    },
    device: deviceStatus(),
    alerts: alerts.status(),
  });
});

app.get('/api/health', (req, res) => {
  res.setHeader('Cache-Control', 'no-store');
  res.json({
    ok: true,
    status: 'healthy',
    uptimeSec: Math.round((Date.now() - startedAt) / 1000),
    now: nowIso(),
  });
});

// ---------------------------------------------------------------------------
// Static dashboard
// ---------------------------------------------------------------------------
app.use(
  express.static(path.join(__dirname, 'public'), {
    extensions: ['html'],
    setHeaders(res, filePath) {
      if (filePath.endsWith('.html')) res.setHeader('Cache-Control', 'no-cache');
    },
  })
);

app.use('/api', (req, res) => res.status(404).json({ ok: false, error: 'Unknown API endpoint' }));

// Central error handler: malformed JSON, payload too large, unexpected throws.
// eslint-disable-next-line no-unused-vars
app.use((err, req, res, next) => {
  if (res.headersSent) return next(err);
  const status = err.status || err.statusCode || 500;
  if (status >= 500) console.error('[error]', err.message);
  res.status(status).json({
    ok: false,
    error: status === 400 ? 'Invalid request body (expected JSON)' : 'Internal server error',
  });
});

// ---------------------------------------------------------------------------
// Socket.IO
// ---------------------------------------------------------------------------
io.on('connection', (socket) => {
  console.log(`[socket] dashboard connected (${socket.id}), clients=${io.engine.clientsCount}`);
  emitSnapshot(socket);

  socket.on('disconnect', (reason) => {
    console.log(`[socket] dashboard disconnected (${reason}), clients=${io.engine.clientsCount}`);
  });
});

// ---------------------------------------------------------------------------
// Device offline watchdog
// ---------------------------------------------------------------------------
setInterval(() => {
  if (!state.deviceOnline) return;
  if (Date.now() - state.lastSeenMs <= OFFLINE_AFTER_MS) return;

  state.deviceOnline = false;
  io.emit('device:status', deviceStatus());
  console.warn(`[device] OFFLINE - no reading for ${OFFLINE_AFTER_MS} ms`);
}, Math.max(1000, Math.min(OFFLINE_AFTER_MS, 5000))).unref();

// ---------------------------------------------------------------------------
// Start / shutdown
// ---------------------------------------------------------------------------
httpServer.listen(PORT, () => {
  console.log('Smart Water Monitoring backend v' + SERVER_VERSION);
  console.log(`  listening on port ${PORT}`);
  console.log(`  device offline threshold: ${OFFLINE_AFTER_MS} ms`);
  console.log(`  CORS origin(s): ${CORS_ORIGINS.join(', ')}`);
  console.log(
    alerts.status().enabled
      ? `  alerts: Telegram enabled (LOW < ${alerts.status().lowPercent}%, FULL >= ${alerts.status().fullPercent}%)`
      : '  alerts: disabled (set TELEGRAM_BOT_TOKEN and TELEGRAM_CHAT_ID to enable)'
  );
  console.log(`  dashboard: http://localhost:${PORT}/`);
});

function shutdown(signal) {
  console.log(`\n[server] ${signal} received - shutting down`);
  io.close();
  httpServer.close(() => process.exit(0));
  setTimeout(() => process.exit(0), 5000).unref();
}

process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('SIGINT', () => shutdown('SIGINT'));
