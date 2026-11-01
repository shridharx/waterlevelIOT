// Serves Dashboard.h's page and stands in for the ESP32 API so the dashboard
// can be clicked through without hardware. Mirrors the firmware's endpoints.
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = parseInt(process.argv[2] || '8080', 10);
const CAP = 10080;
const WEEK = 168 * 60;

const EMPTY_CM = 19.6;
const FULL_CM = 3.0;

const nowMin = Math.floor(Date.now() / 60000);

// One stored minute per point for a full week, so 1H/6H/24H/7D all have data.
let series = [];
for (let i = WEEK - 1; i >= 0; i--) {
  const pct = Math.max(4, Math.min(96,
    Math.round(63 + 9 * Math.sin(i / 260) + 3 * Math.sin(i / 37))));
  series.push({ m: nowMin - i, pct });
}

const cmFor = (pct) => EMPTY_CM - (pct / 100) * (EMPTY_CM - FULL_CM);
const clamp = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);

function livePercent() {
  // ~60 s cycle plus a faster wobble, so a 2-second poll visibly moves
  // (the old 90 s period drifted ~0.1% per poll and looked frozen).
  const t = Date.now();
  return Math.round(63 + 7 * Math.sin(t / 9500) + 2 * Math.sin(t / 1700));
}

function historyPayload(hours, maxPoints) {
  const windowMin = hours * 60;
  const fromMin = nowMin > windowMin ? nowMin - windowMin : 0;
  const span = Math.max(1, nowMin - fromMin);
  const step = Math.max(1, Math.ceil(span / maxPoints));

  const pts = [];
  let bucketId = -1, bucketStart = 0, sumPct = 0, n = 0, minPct = 255, maxPct = 0;

  const flush = () => {
    if (!n) return;
    pts.push([bucketStart * 60, Math.round(sumPct / n), minPct, maxPct,
      Number(cmFor(sumPct / n).toFixed(1))]);
  };

  for (const p of series) {
    if (p.m < fromMin || p.m > nowMin) continue;
    const id = Math.floor((p.m - fromMin) / step);
    if (id !== bucketId) {
      flush();
      bucketId = id;
      bucketStart = fromMin + id * step;
      sumPct = 0; n = 0; minPct = 255; maxPct = 0;
    }
    sumPct += p.pct; n++;
    if (p.pct < minPct) minPct = p.pct;
    if (p.pct > maxPct) maxPct = p.pct;
  }
  flush();

  return { ok: true, synthetic: false, nowMinute: nowMin, step, hours, points: pts };
}

function csvPayload(hours) {
  const fromMin = nowMin > hours * 60 ? nowMin - hours * 60 : 0;
  let out = 'time_base,epoch_seconds,utc_time,level_percent,distance_cm\r\n';
  for (const p of series) {
    if (p.m < fromMin) continue;
    out += 'epoch,' + p.m * 60 + ',' + new Date(p.m * 60000).toISOString().replace(/\.\d+Z$/, 'Z') +
      ',' + p.pct + ',' + Math.round(cmFor(p.pct)) + '\r\n';
  }
  return out;
}

const page = fs.readFileSync(path.join(__dirname, 'dashboard.html'), 'utf8');

http.createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');
  const q = url.searchParams;

  if (req.method === 'POST' && url.pathname === '/api/clear') {
    series = [];
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end('{"ok":true}');
  }

  if (url.pathname === '/api/live') {
    const pct = livePercent();
    res.writeHead(200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
    return res.end(JSON.stringify({
      ok: true,
      percent: pct,
      cm: Number(cmFor(pct).toFixed(1)),
      samples: 5,
      sensor: true,
      wifi: true,
      blynk: true,
      rssi: -58,
      ip: '192.168.1.50',
      uptime: Math.round(process.uptime() + 3725),
      time: Math.floor(Date.now() / 1000),
      timeValid: true,
      high: 80,
      alert: pct >= 80,
      history: series.length,
      capacity: CAP,
    }));
  }

  if (url.pathname === '/api/history') {
    const hours = clamp(parseInt(q.get('hours') || '24', 10) || 24, 1, 168);
    const points = clamp(parseInt(q.get('points') || '600', 10) || 600, 60, 1200);
    res.writeHead(200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
    return res.end(JSON.stringify(historyPayload(hours, points)));
  }

  if (url.pathname === '/api/export.csv') {
    const hours = clamp(parseInt(q.get('hours') || '168', 10) || 168, 1, 168);
    res.writeHead(200, {
      'Content-Type': 'text/csv; charset=utf-8',
      'Content-Disposition': 'attachment; filename="water-level.csv"',
    });
    return res.end(csvPayload(hours));
  }

  if (url.pathname === '/favicon.ico') { res.writeHead(204); return res.end(); }

  if (url.pathname === '/') {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
    return res.end(page);
  }

  res.writeHead(404, { 'Content-Type': 'application/json' });
  res.end('{"ok":false,"error":"not found"}');
}).listen(PORT, () => console.log('AquaLevel dashboard on http://localhost:' + PORT));
