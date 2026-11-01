'use strict';

/**
 * Demo / test device simulator.
 * Sends realistic readings to the backend every 2 seconds so the dashboard can
 * be demonstrated without the physical ESP32 (useful for the college demo and
 * for checking the API before the hardware is ready).
 *
 * Usage (from the backend folder, with .env containing DEVICE_API_KEY):
 *   npm run simulate
 *   npm run simulate -- https://your-service.onrender.com
 *
 * This is a development tool. Do not deploy it.
 */

require('dotenv').config();

const BASE_URL = (process.argv[2] || `http://localhost:${process.env.PORT || 3000}`).replace(/\/$/, '');
const API_KEY = process.env.DEVICE_API_KEY || '';
const DEVICE_ID = process.env.DEVICE_ID || 'esp32-water-01';

const TANK_HEIGHT_CM = 19;
const EMPTY_DISTANCE_CM = 19;
const INTERVAL_MS = 2000;

if (!API_KEY) {
  console.error('DEVICE_API_KEY is not set. Create backend/.env (copy .env.example).');
  process.exit(1);
}

const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
let tick = 0;
let percent = 45;

async function send() {
  tick += 1;
  // Slow drain/fill pattern between 10 % and 90 %.
  percent = clamp(50 + 40 * Math.sin(tick / 25), 5, 98);
  const waterHeightCm = (percent / 100) * TANK_HEIGHT_CM;
  const distanceCm = clamp(EMPTY_DISTANCE_CM - waterHeightCm, 2, 60);

  const body = {
    deviceId: DEVICE_ID,
    valid: true,
    distanceCm: Number(distanceCm.toFixed(1)),
    tankHeightCm: TANK_HEIGHT_CM,
    emptyDistanceCm: EMPTY_DISTANCE_CM,
    samples: 5,
    rssi: -55 - Math.round(Math.random() * 12),
    uptimeSec: tick * 2,
    firmware: 'sim-1.0.0',
  };

  try {
    const res = await fetch(`${BASE_URL}/api/water-level`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'x-api-key': API_KEY },
      body: JSON.stringify(body),
    });
    const text = await res.text();
    console.log(`${new Date().toLocaleTimeString()} -> ${res.status} ${text}`);
  } catch (err) {
    console.error(`${new Date().toLocaleTimeString()} -> network error: ${err.message}`);
  }
}

console.log(`Simulating ${DEVICE_ID} -> ${BASE_URL}/api/water-level every ${INTERVAL_MS / 1000}s`);
send();
setInterval(send, INTERVAL_MS);
