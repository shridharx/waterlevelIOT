/* ==========================================================================
   AquaSense dashboard client
   - receives live readings over Socket.IO
   - falls back to REST polling when the socket is down
   - keeps history in browser memory only (no database)
   ========================================================================== */

'use strict';

const MAX_POINTS = 120; // 120 readings x 2 s = 4 minutes
const LEVELS = [
  { key: 'empty', min: 0, label: 'EMPTY', note: 'Tank is empty. Refill immediately.' },
  { key: 'low', min: 5, label: 'LOW', note: 'Low water level. Plan a refill soon.' },
  { key: 'medium', min: 25, label: 'MEDIUM', note: 'Water level is within the normal range.' },
  { key: 'high', min: 60, label: 'HIGH', note: 'Tank is well filled.' },
  { key: 'full', min: 90, label: 'FULL', note: 'Tank is almost full — stop filling.' },
];

const state = {
  online: false,
  socketConnected: false,
  lastReadingAt: 0,
  sensorOk: true,
  offlineAfterMs: 15000,
  history: [], // { t: Date, percent: number }
  polling: false,
};

const $ = (id) => document.getElementById(id);

const el = {
  gauge: $('gauge'),
  gaugeFill: $('gaugeFill'),
  gaugeProgress: $('gaugeProgress'),
  levelPercent: $('levelPercent'),
  gaugeCaption: $('gaugeCaption'),
  levelHint: $('levelHint'),
  statusChip: $('statusChip'),
  statusNote: $('statusNote'),
  scale: $('statusScale'),
  sensorAlert: $('sensorAlert'),
  sensorAlertText: $('sensorAlertText'),
  lastUpdated: $('lastUpdated'),
  socketPill: $('socketPill'),
  socketText: $('socketText'),
  devicePill: $('devicePill'),
  deviceText: $('deviceText'),
  waterHeight: $('waterHeight'),
  distanceCm: $('distanceCm'),
  tankHeight: $('tankHeight'),
  emptyDistance: $('emptyDistance'),
  samples: $('samples'),
  capacity: $('capacity'),
  rowDevice: $('rowDevice'),
  rowWifi: $('rowWifi'),
  rowLastSensor: $('rowLastSensor'),
  rowServer: $('rowServer'),
  rowUptime: $('rowUptime'),
  rowFirmware: $('rowFirmware'),
};

/* ------------------------------- helpers -------------------------------- */

function fmt(value, decimals = 1) {
  return Number.isFinite(value) ? value.toFixed(decimals) : '—';
}

function levelFor(percent) {
  if (!Number.isFinite(percent)) return null;
  let match = LEVELS[0];
  for (const level of LEVELS) if (percent >= level.min) match = level;
  return match;
}

function timeText(date) {
  return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

function relativeTime(ms) {
  if (!ms) return '—';
  const seconds = Math.max(0, Math.round((Date.now() - ms) / 1000));
  if (seconds < 2) return 'just now';
  if (seconds < 60) return `${seconds}s ago`;
  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes}m ago`;
  return `${Math.floor(minutes / 60)}h ago`;
}

function wifiQuality(rssi) {
  if (!Number.isFinite(rssi)) return { text: '—', cls: '' };
  const label = rssi >= -60 ? 'Excellent' : rssi >= -70 ? 'Good' : rssi >= -80 ? 'Fair' : 'Weak';
  return { text: `${rssi} dBm · ${label}`, cls: rssi >= -70 ? 'is-good' : rssi >= -80 ? 'is-warn' : 'is-bad' };
}

function uptimeText(seconds) {
  if (!Number.isFinite(seconds)) return '—';
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  if (d > 0) return `${d}d ${h}h`;
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m ${seconds % 60}s`;
}

/* --------------------------- gauge + status ----------------------------- */

function paintReading(reading) {
  if (!reading) return;

  state.lastReadingAt = Date.parse(reading.receivedAt) || Date.now();
  const percent = reading.percent;
  const level = levelFor(percent);
  const levelKey = level ? level.key : 'unknown';

  // Gauge ring + animated water fill
  const circumference = 2 * Math.PI * 88;
  el.gaugeProgress.style.strokeDashoffset = String(
    circumference * (1 - Math.min(100, Math.max(0, percent)) / 100)
  );
  el.gaugeFill.style.height = `${Math.min(100, Math.max(0, percent))}%`;
  el.gauge.dataset.level = levelKey;
  el.levelPercent.textContent = fmt(percent, 0);

  el.gaugeCaption.textContent = `${fmt(reading.waterHeightCm)} cm of water in the tank`;
  el.levelHint.textContent = `Tank height ${fmt(reading.tankHeightCm)} cm`;

  // Status chip + scale
  el.statusChip.dataset.level = levelKey;
  el.statusChip.textContent = level ? level.label : 'UNKNOWN';
  el.statusNote.textContent = level ? level.note : 'Waiting for a valid reading.';
  for (const seg of el.scale.querySelectorAll('.scale__seg')) {
    seg.classList.toggle('is-active', seg.dataset.level === levelKey);
  }

  // Sensor tiles
  el.waterHeight.textContent = fmt(reading.waterHeightCm);
  el.distanceCm.textContent = fmt(reading.distanceCm);
  el.tankHeight.textContent = fmt(reading.tankHeightCm);
  el.emptyDistance.textContent = fmt(reading.emptyDistanceCm);
  el.samples.textContent = Number.isFinite(reading.samples) ? String(reading.samples) : '—';
  el.capacity.textContent = fmt(percent, 0);

  // Device rows
  const wifi = wifiQuality(reading.rssi);
  el.rowWifi.textContent = wifi.text;
  el.rowWifi.className = wifi.cls;
  el.rowUptime.textContent = uptimeText(reading.uptimeSec);
  el.rowFirmware.textContent = reading.firmware || '—';
  el.rowLastSensor.textContent = timeText(new Date(state.lastReadingAt));

  // Sensor alert clears as soon as a healthy reading arrives
  state.sensorOk = true;
  el.sensorAlert.hidden = true;

  pushHistory(percent);
}

function paintDevice(status) {
  if (!status) return;
  if (Number.isFinite(status.offlineAfterMs)) state.offlineAfterMs = status.offlineAfterMs;
  state.online = Boolean(status.online);
  paintOnlineBadge();
}

function paintOnlineBadge() {
  const stale = state.lastReadingAt > 0 && Date.now() - state.lastReadingAt > state.offlineAfterMs;
  const online = state.online && !stale;

  el.devicePill.classList.toggle('is-online', online);
  el.devicePill.classList.toggle('is-offline', !online);
  el.deviceText.textContent = online ? 'ESP32 online' : 'ESP32 offline';
  el.rowDevice.textContent = online ? 'Online' : 'Offline';
  el.rowDevice.className = online ? 'is-good' : 'is-bad';
}

/* -------------------------------- chart --------------------------------- */

let chart = null;

function chartColors() {
  const css = getComputedStyle(document.documentElement);
  return {
    grid: css.getPropertyValue('--chart-grid').trim() || 'rgba(142,190,255,0.08)',
    ticks: css.getPropertyValue('--muted').trim() || '#8ba3c2',
    line: css.getPropertyValue('--accent').trim() || '#38bdf8',
  };
}

function initChart() {
  const canvas = document.getElementById('levelChart');
  if (!canvas || typeof Chart === 'undefined') return;
  const colors = chartColors();

  chart = new Chart(canvas.getContext('2d'), {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        {
          label: 'Water level',
          data: [],
          borderColor: colors.line,
          borderWidth: 2.5,
          tension: 0.35,
          pointRadius: 0,
          pointHoverRadius: 4,
          pointHoverBackgroundColor: colors.line,
          fill: true,
          backgroundColor: (context) => {
            const { ctx, chartArea } = context.chart;
            if (!chartArea) return 'transparent';
            const gradient = ctx.createLinearGradient(0, chartArea.top, 0, chartArea.bottom);
            gradient.addColorStop(0, 'rgba(56, 189, 248, 0.35)');
            gradient.addColorStop(1, 'rgba(56, 189, 248, 0)');
            return gradient;
          },
        },
      ],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: { duration: 350 },
      interaction: { intersect: false, mode: 'index' },
      plugins: {
        legend: { display: false },
        tooltip: {
          backgroundColor: 'rgba(6, 20, 36, 0.92)',
          borderColor: 'rgba(56, 189, 248, 0.35)',
          borderWidth: 1,
          padding: 10,
          displayColors: false,
          callbacks: {
            label: (item) => `${item.parsed.y.toFixed(1)} %`,
          },
        },
      },
      scales: {
        x: {
          grid: { display: false },
          ticks: { color: colors.ticks, maxTicksLimit: 6, maxRotation: 0, font: { size: 11 } },
          border: { color: colors.grid },
        },
        y: {
          min: 0,
          max: 100,
          grid: { color: colors.grid },
          border: { display: false },
          ticks: {
            color: colors.ticks,
            stepSize: 25,
            font: { size: 11 },
            callback: (value) => `${value}%`,
          },
        },
      },
    },
  });
}

function pushHistory(percent) {
  if (!Number.isFinite(percent) || !chart) return;
  const now = new Date();
  const last = state.history[state.history.length - 1];
  // Reading every 2 s; ignore duplicates that arrive faster than 500 ms.
  if (last && now - last.t < 500) {
    last.percent = percent;
    last.t = now;
  } else {
    state.history.push({ t: now, percent });
  }
  while (state.history.length > MAX_POINTS) state.history.shift();

  chart.data.labels = state.history.map((p) => timeText(p.t));
  chart.data.datasets[0].data = state.history.map((p) => p.percent);
  chart.update('none');
}

function applyChartTheme() {
  if (!chart) return;
  const colors = chartColors();
  chart.data.datasets[0].borderColor = colors.line;
  chart.options.scales.x.ticks.color = colors.ticks;
  chart.options.scales.y.ticks.color = colors.ticks;
  chart.options.scales.y.grid.color = colors.grid;
  chart.options.scales.x.border.color = colors.grid;
  chart.update('none');
}

/* ------------------------------ connection ------------------------------ */

function paintSocket(connected, label) {
  state.socketConnected = connected;
  el.socketPill.classList.toggle('is-online', connected);
  el.socketPill.classList.toggle('is-waiting', !connected);
  el.socketText.textContent = label || (connected ? 'Live' : 'Reconnecting…');
  el.rowServer.textContent = connected ? 'Connected (Socket.IO)' : 'Reconnecting…';
  el.rowServer.className = connected ? 'is-good' : 'is-warn';
}

function startPolling() {
  if (state.polling) return;
  state.polling = true;
  const poll = async () => {
    try {
      const res = await fetch('/api/water-level', { cache: 'no-store' });
      const json = await res.json();
      if (json.data) paintReading(json.data);
      if (json.device) paintDevice(json.device);
    } catch {
      /* keep trying silently */
    }
  };
  poll();
  setInterval(poll, 5000);
}

/* -------------------------------- socket -------------------------------- */

if (typeof io === 'function') {
  const socket = io({ transports: ['websocket', 'polling'] });

  socket.on('connect', () => {
    paintSocket(true, 'Live');
  });

  socket.on('disconnect', () => {
    paintSocket(false, 'Reconnecting…');
    startPolling();
  });

  socket.on('connect_error', () => {
    paintSocket(false, 'Reconnecting…');
    startPolling();
  });

  socket.on('meta', (meta) => {
    if (meta && Number.isFinite(meta.offlineAfterMs)) state.offlineAfterMs = meta.offlineAfterMs;
  });

  socket.on('water:update', paintReading);
  socket.on('device:status', paintDevice);
  socket.on('sensor:error', (event) => {
    state.sensorOk = false;
    el.sensorAlert.hidden = false;
    el.sensorAlertText.textContent = `${event.message || 'No valid echo'} (${relativeTime(Date.parse(event.at))})`;
  });
} else {
  paintSocket(false, 'Offline mode');
  startPolling();
}

/* -------------------------- initial + housekeeping ----------------------- */

(async function loadInitial() {
  try {
    const res = await fetch('/api/water-level', { cache: 'no-store' });
    const json = await res.json();
    if (json.data) paintReading(json.data);
    if (json.device) paintDevice(json.device);
  } catch {
    /* socket will deliver as soon as it connects */
  }
})();

setInterval(() => {
  if (state.lastReadingAt) {
    el.lastUpdated.textContent = relativeTime(state.lastReadingAt);
    el.lastUpdated.title = new Date(state.lastReadingAt).toLocaleString();
  }
  paintOnlineBadge();
}, 1000);

/* --------------------------------- theme -------------------------------- */

const themeToggle = $('themeToggle');
const storedTheme = localStorage.getItem('aquasense-theme');
if (storedTheme === 'light' || storedTheme === 'dark') {
  document.documentElement.dataset.theme = storedTheme;
}

themeToggle.addEventListener('click', () => {
  const next = document.documentElement.dataset.theme === 'light' ? 'dark' : 'light';
  document.documentElement.dataset.theme = next;
  localStorage.setItem('aquasense-theme', next);
  applyChartTheme();
});

/* --------------------------------- boot --------------------------------- */

initChart();
paintOnlineBadge();
