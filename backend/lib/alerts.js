'use strict';

/**
 * LOW / FULL water level alerts.
 * -------------------------------------------------
 * Sends a message to a Telegram chat (free Bot API, no account plan, no
 * verified domain, no database) when the tank crosses a threshold.
 *
 * Behaviour
 *   - LOW    when the level falls below ALERT_LOW_PERCENT (default 25 %)
 *   - FULL   when the level reaches ALERT_FULL_PERCENT (default 90 %)
 *   - Hysteresis (default 2 %) stops a level hovering on a threshold from
 *     sending a message every 2 seconds.
 *   - A cooldown (default 10 min) limits repeated LOW/FULL messages.
 *   - Alerts are sent only on a transition into a zone, never on every reading.
 *   - If TELEGRAM_BOT_TOKEN / TELEGRAM_CHAT_ID are not set, alerts are simply
 *     disabled: the server and dashboard keep working.
 */

const num = (value, fallback) => {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
};

const config = {
  botToken: (process.env.TELEGRAM_BOT_TOKEN || '').trim(),
  chatId: (process.env.TELEGRAM_CHAT_ID || '').trim(),
  // Override only for testing against a mock Bot API.
  apiBase: (process.env.TELEGRAM_API_BASE || 'https://api.telegram.org').replace(/\/+$/, ''),
  lowPercent: num(process.env.ALERT_LOW_PERCENT, 25),
  fullPercent: num(process.env.ALERT_FULL_PERCENT, 90),
  hysteresis: num(process.env.ALERT_HYSTERESIS_PERCENT, 2),
  cooldownMs: num(process.env.ALERT_COOLDOWN_MS, 10 * 60 * 1000),
  disabled: ['0', 'false', 'off', 'no'].includes(
    String(process.env.ALERTS_ENABLED || '').toLowerCase()
  ),
};

const enabled = Boolean(config.botToken && config.chatId) && !config.disabled;

let zone = 'ok'; // 'ok' | 'low' | 'full'
let lastSentAt = 0;
let lastSentZone = null;
let sentCount = 0;

/**
 * Zone after this reading, using hysteresis so a level sitting on a threshold
 * does not flap between states.
 */
function nextZone(current, percent) {
  if (current === 'low') {
    if (percent >= config.lowPercent + config.hysteresis) {
      return percent >= config.fullPercent ? 'full' : 'ok';
    }
    return 'low';
  }
  if (current === 'full') {
    if (percent <= config.fullPercent - config.hysteresis) {
      return percent < config.lowPercent ? 'low' : 'ok';
    }
    return 'full';
  }
  if (percent < config.lowPercent) return 'low';
  if (percent >= config.fullPercent) return 'full';
  return 'ok';
}

function buildMessage(reading, target) {
  const label =
    target === 'low'
      ? 'LOW WATER LEVEL — refill soon'
      : 'TANK FULL — stop filling';
  const percent = Number(reading.percent).toFixed(1);
  const height = Number(reading.waterHeightCm).toFixed(1);
  const tank = Number(reading.tankHeightCm).toFixed(1);
  const distance = Number(reading.distanceCm).toFixed(1);

  return [
    `AquaSense alert: ${label}`,
    '',
    `Level: ${percent}%`,
    `Water height: ${height} cm of ${tank} cm`,
    `Distance from sensor: ${distance} cm`,
    `Device: ${reading.deviceId}`,
    `Time: ${reading.receivedAt || new Date().toISOString()}`,
  ].join('\n');
}

async function sendTelegram(text) {
  const url = `${config.apiBase}/bot${config.botToken}/sendMessage`;
  const res = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      chat_id: config.chatId,
      text,
      disable_web_page_preview: true,
    }),
    signal: AbortSignal.timeout(8000),
  });

  const body = await res.json().catch(() => ({}));
  if (!res.ok || body.ok === false) {
    throw new Error(body.description || `Telegram responded HTTP ${res.status}`);
  }
}

/**
 * Call once per accepted reading (not per ping). Never throws.
 * Returns { zone, sent, skipped?, error? }.
 */
async function evaluateReading(reading) {
  if (!reading || !Number.isFinite(reading.percent)) {
    return { zone, sent: false };
  }

  const previous = zone;
  zone = nextZone(previous, reading.percent);

  if (!enabled || zone === previous || zone === 'ok') {
    return { zone, sent: false };
  }

  if (Date.now() - lastSentAt < config.cooldownMs) {
    console.log(`[alerts] ${zone.toUpperCase()} in cooldown, not sending`);
    return { zone, sent: false, skipped: 'cooldown' };
  }

  // Reserve the slot before the network call so parallel readings cannot
  // produce duplicate messages.
  lastSentAt = Date.now();
  lastSentZone = zone;

  try {
    await sendTelegram(buildMessage(reading, zone));
    sentCount += 1;
    console.log(`[alerts] sent ${zone.toUpperCase()} alert (${reading.percent.toFixed(1)}%)`);
    return { zone, sent: true };
  } catch (err) {
    console.error('[alerts] Telegram error:', err.message);
    return { zone, sent: false, error: err.message };
  }
}

function status() {
  return {
    enabled,
    channel: enabled ? 'telegram' : null,
    lowPercent: config.lowPercent,
    fullPercent: config.fullPercent,
    hysteresisPercent: config.hysteresis,
    cooldownMs: config.cooldownMs,
    zone,
    lastSentAt: lastSentAt ? new Date(lastSentAt).toISOString() : null,
    lastSentZone,
    sentCount,
  };
}

module.exports = { evaluateReading, status, config };
