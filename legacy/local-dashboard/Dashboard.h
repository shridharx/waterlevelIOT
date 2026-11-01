#pragma once
#include <Arduino.h>

// =====================================================
// AQUALEVEL DASHBOARD
// Single-page UI served from ESP32 flash (PROGMEM).
// No external CDN: works fully offline on your LAN.
// =====================================================

const char DASHBOARD_HTML[] PROGMEM = R"WLHTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#050f1a">
<title>AquaLevel - Water Monitor</title>
<style>
  :root{
    --bg:#050f1a;
    --bg2:#0a1b2c;
    --panel:rgba(255,255,255,.045);
    --line:rgba(125,211,252,.14);
    --text:#e9f5ff;
    --muted:#7f9bb3;
    --accent:#38bdf8;
    --good:#34d399;
    --warn:#fbbf24;
    --bad:#fb7185;
    --level-rgb:56,189,248;
    --radius:20px;
  }
  html[data-theme="light"]{
    --bg:#eef5fb;
    --bg2:#e2edf7;
    --panel:rgba(255,255,255,.72);
    --line:rgba(2,132,199,.16);
    --text:#0b2239;
    --muted:#5b7891;
    --accent:#0284c7;
    --good:#059669;
    --warn:#d97706;
    --bad:#dc2626;
  }
  *{box-sizing:border-box}
  html,body{min-height:100%}
  body{
    margin:0;
    font-family:ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
    color:var(--text);
    background:
      radial-gradient(1100px 650px at 8% -10%,rgba(34,211,238,.15),transparent 60%),
      radial-gradient(900px 600px at 100% 0%,rgba(56,189,248,.13),transparent 55%),
      radial-gradient(800px 800px at 50% 120%,rgba(14,116,144,.18),transparent 60%),
      linear-gradient(180deg,var(--bg),var(--bg2));
    background-color:var(--bg);
    -webkit-font-smoothing:antialiased;
  }
  html[data-theme="light"] body{
    background:
      radial-gradient(1100px 650px at 8% -10%,rgba(14,165,233,.13),transparent 60%),
      radial-gradient(900px 600px at 100% 0%,rgba(56,189,248,.11),transparent 55%),
      radial-gradient(800px 800px at 50% 120%,rgba(14,116,144,.10),transparent 60%),
      linear-gradient(180deg,var(--bg),var(--bg2));
    background-color:var(--bg);
  }
  .topbar{
    max-width:1180px;margin:0 auto;padding:26px 20px 6px;
    display:flex;align-items:center;justify-content:space-between;gap:16px;
  }
  .brand{display:flex;align-items:center;gap:14px}
  .logo{
    width:44px;height:44px;border-radius:14px;display:grid;place-items:center;
    color:#04121f;background:linear-gradient(135deg,#67e8f9,#38bdf8);
    box-shadow:0 10px 30px rgba(56,189,248,.35);flex:none;
  }
  .logo svg{width:24px;height:24px}
  .brand h1{margin:0;font-size:19px;letter-spacing:.2px}
  .brand p{margin:2px 0 0;font-size:11px;color:var(--muted);letter-spacing:.14em;text-transform:uppercase}
  .top-right{display:flex;align-items:center;gap:12px}
  .icon-btn{
    width:38px;height:38px;flex:none;display:grid;place-items:center;
    border:1px solid var(--line);border-radius:12px;background:rgba(255,255,255,.05);
    color:var(--muted);cursor:pointer;transition:.18s;
  }
  .icon-btn:hover{color:var(--text);border-color:rgba(125,211,252,.35);background:rgba(255,255,255,.08)}
  .icon-btn svg{width:19px;height:19px}
  .pill{
    display:flex;align-items:center;gap:8px;padding:8px 14px;border-radius:999px;
    font-size:12.5px;font-weight:600;letter-spacing:.2px;
    background:rgba(255,255,255,.05);border:1px solid var(--line);color:var(--muted);
    transition:.25s;
  }
  .pill .dot{width:8px;height:8px;border-radius:50%;background:var(--warn);box-shadow:0 0 0 4px rgba(251,191,36,.12)}
  .pill.online{color:#bff7e4;border-color:rgba(52,211,153,.3);background:rgba(52,211,153,.08)}
  .pill.online .dot{background:var(--good);box-shadow:0 0 0 4px rgba(52,211,153,.15);animation:pulse 2s infinite}
  .pill.offline{color:#ffd3db;border-color:rgba(251,113,133,.3);background:rgba(251,113,133,.08)}
  .pill.offline .dot{background:var(--bad);box-shadow:0 0 0 4px rgba(251,113,133,.15)}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.45}}
  .clock{font-variant-numeric:tabular-nums;font-size:13px;color:var(--muted)}
  .wrap{max-width:1180px;margin:0 auto;padding:22px 20px 40px;display:grid;gap:18px}
  .card{
    background:linear-gradient(180deg,var(--panel),rgba(255,255,255,.02));
    border:1px solid var(--line);border-radius:var(--radius);
    box-shadow:0 24px 60px rgba(2,8,20,.35),inset 0 1px 0 rgba(255,255,255,.05);
    backdrop-filter:blur(14px);
  }
  .hero{display:grid;grid-template-columns:auto 1fr;gap:44px;align-items:center;padding:36px 40px}
  .tank-wrap{display:flex;align-items:stretch;gap:16px}
  .tank{
    position:relative;width:156px;height:250px;border-radius:28px;overflow:hidden;
    background:linear-gradient(180deg,rgba(255,255,255,.06),rgba(255,255,255,.01));
    border:1px solid rgba(125,211,252,.22);
    box-shadow:inset 0 0 46px rgba(2,8,20,.65),0 26px 60px rgba(2,8,20,.45);
  }
  .tank::after{
    content:"";position:absolute;left:15px;top:14px;width:9px;height:62%;
    border-radius:20px;background:linear-gradient(180deg,rgba(255,255,255,.20),transparent);
    filter:blur(2px);pointer-events:none;
  }
  .tank-mark{
    position:absolute;left:0;right:0;bottom:0;height:2px;z-index:3;
    background:rgba(255,255,255,.75);
    box-shadow:0 0 12px rgba(255,255,255,.55);
    transition:bottom .9s cubic-bezier(.22,1,.36,1);
  }
  .tank-high{
    position:absolute;left:0;right:0;bottom:80%;height:1px;z-index:2;
    background:var(--bad);opacity:.55;
  }
  html[data-theme="light"] .tank-mark{background:#fff;box-shadow:0 0 10px rgba(2,132,199,.55)}
  .water{
    position:absolute;left:0;right:0;bottom:0;height:0;
    background:linear-gradient(180deg,rgba(var(--level-rgb),.78),rgba(var(--level-rgb),.28));
    transition:height .9s cubic-bezier(.22,1,.36,1);
    box-shadow:0 -10px 34px rgba(var(--level-rgb),.35);
  }
  .wave{position:absolute;left:0;width:200%;height:24px;top:-13px;display:block}
  .wave path{fill:rgba(var(--level-rgb),.78)}
  .wave.w1{animation:waveMove 7s linear infinite}
  .wave.w2{top:-8px;opacity:.45;animation:waveMove 11s linear infinite reverse}
  @keyframes waveMove{from{transform:translateX(0)}to{transform:translateX(-50%)}}
  .ticks{position:relative;width:40px;height:250px}
  .ticks span{
    position:absolute;left:0;transform:translateY(50%);
    font-size:10.5px;color:var(--muted);font-variant-numeric:tabular-nums;
  }
  .ticks span::before{
    content:"";position:absolute;left:-9px;top:50%;width:6px;height:1px;
    background:rgba(125,211,252,.35);
  }
  .eyebrow{font-size:11.5px;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);font-weight:600}
  .big{
    font-size:clamp(56px,9vw,92px);font-weight:700;line-height:1;letter-spacing:-.04em;
    margin:8px 0 6px;font-variant-numeric:tabular-nums;color:#fff;
    text-shadow:0 0 40px rgba(var(--level-rgb),.35);
  }
  .big span{font-size:.38em;font-weight:600;color:var(--muted);margin-left:4px;letter-spacing:0}
  .sub{color:var(--muted);font-size:14px;margin-bottom:18px}
  .chips{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:22px}
  .chip{
    display:inline-flex;align-items:center;gap:7px;padding:7px 12px;border-radius:999px;
    font-size:12px;font-weight:600;border:1px solid var(--line);
    background:rgba(255,255,255,.04);color:#cfe6f7;
  }
  .chip .dot{width:7px;height:7px;border-radius:50%;background:currentColor}
  .chip.ok{color:#8ef0cd;border-color:rgba(52,211,153,.28);background:rgba(52,211,153,.08)}
  .chip.warn{color:#ffe4a3;border-color:rgba(251,191,36,.3);background:rgba(251,191,36,.08)}
  .chip.bad{color:#ffb3c0;border-color:rgba(251,113,133,.3);background:rgba(251,113,133,.08)}
  .hero-meta{display:flex;gap:34px;flex-wrap:wrap}
  .hero-meta div{display:flex;flex-direction:column;gap:3px}
  .hero-meta span{font-size:10.5px;text-transform:uppercase;letter-spacing:.12em;color:var(--muted)}
  .hero-meta strong{font-size:15px;font-weight:600;font-variant-numeric:tabular-nums}
  .stats{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:14px}
  .stat{padding:16px 18px;display:flex;flex-direction:column;gap:5px;position:relative;overflow:hidden}
  .stat::before{
    content:"";position:absolute;left:0;top:0;width:100%;height:2px;
    background:linear-gradient(90deg,rgba(56,189,248,.6),transparent);
  }
  .stat-label{font-size:10.5px;text-transform:uppercase;letter-spacing:.12em;color:var(--muted);font-weight:600}
  .stat-value{font-size:19px;font-weight:650;font-variant-numeric:tabular-nums}
  .stat-value.ok{color:#8ef0cd}
  .stat-value.bad{color:#ffb3c0}
  .stat-sub{font-size:11.5px;color:var(--muted);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
  .chart{padding:24px 26px 18px}
  .chart-head{display:flex;justify-content:space-between;align-items:flex-start;gap:16px;margin-bottom:8px;flex-wrap:wrap}
  .chart-head h2{margin:0;font-size:16px;letter-spacing:.2px}
  .chart-head p{margin:4px 0 0;font-size:12.5px;color:var(--muted)}
  .tabs{display:flex;gap:4px;padding:4px;border-radius:12px;background:rgba(255,255,255,.04);border:1px solid var(--line)}
  .tabs button{
    border:0;background:transparent;color:var(--muted);font:inherit;font-size:12px;font-weight:600;
    padding:7px 13px;border-radius:9px;cursor:pointer;transition:.18s;
  }
  .tabs button:hover{color:#dcefff}
  .tabs button.active{background:linear-gradient(135deg,#38bdf8,#22d3ee);color:#04121f;box-shadow:0 6px 18px rgba(56,189,248,.3)}
  .chart-wrap{position:relative;height:340px;margin:6px 0 4px}
  #chart{width:100%;height:100%;display:block;touch-action:none}
  .tooltip{
    position:absolute;display:none;pointer-events:none;z-index:5;min-width:150px;
    padding:10px 12px;border-radius:12px;background:rgba(6,18,31,.94);
    border:1px solid rgba(125,211,252,.22);box-shadow:0 16px 40px rgba(0,0,0,.5);
    backdrop-filter:blur(8px);font-size:12px;
  }
  .tip-time{color:var(--muted);font-size:11px;margin-bottom:6px;letter-spacing:.04em}
  .tip-row{display:flex;justify-content:space-between;gap:14px;padding:2px 0}
  .tip-row span{color:var(--muted)}
  .tip-row b{font-variant-numeric:tabular-nums}
  .empty{position:absolute;inset:0;display:none;place-items:center;color:var(--muted);font-size:13px;text-align:center;padding:20px}
  .summary{
    display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-top:8px;
    padding-top:16px;border-top:1px solid rgba(125,211,252,.1);
  }
  .summary div{display:flex;flex-direction:column;gap:3px}
  .summary span{font-size:10.5px;text-transform:uppercase;letter-spacing:.12em;color:var(--muted);font-weight:600}
  .summary strong{font-size:16px;font-variant-numeric:tabular-nums}
  .summary strong.up{color:#8ef0cd}
  .summary strong.down{color:#ffb3c0}
  footer{
    display:flex;justify-content:space-between;align-items:center;gap:16px;
    color:var(--muted);font-size:12px;padding:4px 4px 0;flex-wrap:wrap;
  }
  .ghost{
    border:1px solid var(--line);background:rgba(255,255,255,.03);color:var(--muted);
    padding:8px 14px;border-radius:10px;font:inherit;font-size:12px;cursor:pointer;transition:.18s;
    display:inline-flex;align-items:center;gap:6px;text-decoration:none;
  }
  .ghost:hover{color:#ffd3db;border-color:rgba(251,113,133,.35);background:rgba(251,113,133,.08)}
  .ghost.export:hover{color:#bae6fd;border-color:rgba(56,189,248,.35);background:rgba(56,189,248,.08)}
  .ghost svg{width:14px;height:14px;flex:none}
  .footer-actions{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
  @media(max-width:980px){
    .stats{grid-template-columns:repeat(3,minmax(0,1fr))}
    .hero{gap:30px;padding:28px}
  }
  @media(max-width:760px){
    .hero{grid-template-columns:1fr;justify-items:center;text-align:center;gap:24px}
    .chips,.hero-meta{justify-content:center}
    .chart-wrap{height:260px}
    .summary{grid-template-columns:repeat(2,1fr)}
  }
  @media(max-width:560px){
    .topbar{padding:18px 14px 4px}
    .brand p{display:none}
    .clock{display:none}
    .wrap{padding:16px 14px 30px}
    .stats{grid-template-columns:repeat(2,minmax(0,1fr))}
    .hero{padding:24px 20px}
    .tank{width:130px;height:210px}
    .ticks{height:210px}
    .chart{padding:18px 16px 14px}
  }
  @media(prefers-reduced-motion:reduce){
    .wave{animation:none!important}
    .pill .dot{animation:none!important}
    *{transition:none!important}
  }
</style>
</head>
<body>
<header class="topbar">
  <div class="brand">
    <div class="logo" aria-hidden="true">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
        <path d="M12 2.5c3.5 4.2 6.5 7.6 6.5 11.2a6.5 6.5 0 1 1-13 0C5.5 10.1 8.5 6.7 12 2.5Z"/>
      </svg>
    </div>
    <div>
      <h1>AquaLevel</h1>
      <p>ESP32 tank monitor</p>
    </div>
  </div>
  <div class="top-right">
    <div class="pill" id="connPill"><span class="dot"></span><span id="connText">Connecting</span></div>
    <div class="clock" id="clock">--:--:--</div>
  </div>
</header>

<main class="wrap">

  <section class="hero card">
    <div class="tank-wrap">
      <div class="tank">
        <div class="water" id="water">
          <svg class="wave w1" viewBox="0 0 1200 120" preserveAspectRatio="none" aria-hidden="true">
            <path d="M0,60 C150,110 350,10 600,60 C850,110 1050,10 1200,60 L1200,120 L0,120 Z"/>
          </svg>
          <svg class="wave w2" viewBox="0 0 1200 120" preserveAspectRatio="none" aria-hidden="true">
            <path d="M0,60 C150,110 350,10 600,60 C850,110 1050,10 1200,60 L1200,120 L0,120 Z"/>
          </svg>
        </div>
      </div>
      <div class="ticks" aria-hidden="true">
        <span style="bottom:100%">100%</span>
        <span style="bottom:75%">75%</span>
        <span style="bottom:50%">50%</span>
        <span style="bottom:25%">25%</span>
        <span style="bottom:0%">0%</span>
      </div>
    </div>

    <div class="hero-readout">
      <div class="eyebrow">Current water level</div>
      <div class="big" id="bigPercent">--<span>%</span></div>
      <div class="sub" id="levelText">Waiting for the first reading...</div>
      <div class="chips">
        <div class="chip" id="alertChip"><span class="dot"></span><span id="alertText">Level normal</span></div>
        <div class="chip" id="sensorChip"><span class="dot"></span><span id="sensorText">Sensor OK</span></div>
      </div>
      <div class="hero-meta">
        <div><span>Distance</span><strong id="distance">--</strong></div>
        <div><span>Updated</span><strong id="updated">--</strong></div>
        <div><span>History</span><strong id="heroHistory">--</strong></div>
      </div>
    </div>
  </section>

  <section class="stats">
    <div class="stat card">
      <span class="stat-label">Sensor</span>
      <strong class="stat-value" id="stSensor">--</strong>
      <span class="stat-sub" id="stSensorSub">HC-SR04</span>
    </div>
    <div class="stat card">
      <span class="stat-label">Echo samples</span>
      <strong class="stat-value" id="stSamples">--</strong>
      <span class="stat-sub">valid readings</span>
    </div>
    <div class="stat card">
      <span class="stat-label">Wi-Fi</span>
      <strong class="stat-value" id="stWifi">--</strong>
      <span class="stat-sub" id="stWifiSub">--</span>
    </div>
    <div class="stat card">
      <span class="stat-label">Blynk</span>
      <strong class="stat-value" id="stBlynk">--</strong>
      <span class="stat-sub">cloud link</span>
    </div>
    <div class="stat card">
      <span class="stat-label">Uptime</span>
      <strong class="stat-value" id="stUptime">--</strong>
      <span class="stat-sub">since boot</span>
    </div>
    <div class="stat card">
      <span class="stat-label">History</span>
      <strong class="stat-value" id="stHistory">--</strong>
      <span class="stat-sub" id="stHistorySub">1-minute averages</span>
    </div>
  </section>

  <section class="chart card">
    <div class="chart-head">
      <div>
        <h2>Water level history</h2>
        <p id="chartSub">Last 24 hours</p>
      </div>
      <div class="tabs" role="tablist" aria-label="History range">
        <button type="button" data-hours="1">1H</button>
        <button type="button" data-hours="6">6H</button>
        <button type="button" data-hours="24" class="active">24H</button>
        <button type="button" data-hours="168">7D</button>
      </div>
    </div>
    <div class="chart-wrap">
      <canvas id="chart" aria-label="Water level history chart" role="img"></canvas>
      <div class="tooltip" id="tooltip"></div>
      <div class="empty" id="chartEmpty">Collecting data...<br>The first point appears after one minute.</div>
    </div>
    <div class="summary">
      <div><span>Min</span><strong id="sMin">--</strong></div>
      <div><span>Average</span><strong id="sAvg">--</strong></div>
      <div><span>Max</span><strong id="sMax">--</strong></div>
      <div><span>Trend</span><strong id="sTrend">--</strong></div>
    </div>
  </section>

  <footer>
    <span id="footerInfo">AquaLevel - ESP32 water monitor</span>
    <div class="footer-actions">
      <a class="ghost export" id="exportBtn" href="/api/export.csv?hours=168" download="water-level.csv">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
          <path d="M12 3v12"/><path d="M7 11l5 5 5-5"/><path d="M4 20h16"/>
        </svg>
        Export CSV
      </a>
      <button type="button" class="ghost" id="clearBtn">Clear history</button>
    </div>
  </footer>

</main>
<script>
(function () {
  'use strict';

  var $ = function (id) { return document.getElementById(id); };

  var state = {
    hours: 24,
    live: null,
    points: [],
    synthetic: false,
    nowMinute: 0,
    hover: -1,
    layout: null
  };

  var COLORS = {
    normal: [56, 189, 248],
    good: [52, 211, 153],
    warn: [251, 191, 36],
    bad: [251, 113, 133]
  };

  function setLevelColor(rgb) {
    document.documentElement.style.setProperty('--level-rgb', rgb.join(','));
  }

  function fmtUptime(sec) {
    sec = Math.max(0, Math.floor(sec || 0));
    var d = Math.floor(sec / 86400);
    var h = Math.floor((sec % 86400) / 3600);
    var m = Math.floor((sec % 3600) / 60);
    if (d > 0) return d + 'd ' + h + 'h';
    if (h > 0) return h + 'h ' + m + 'm';
    return m + 'm ' + (sec % 60) + 's';
  }

  function setOnline(on) {
    var pill = $('connPill');
    pill.className = 'pill ' + (on ? 'online' : 'offline');
    $('connText').textContent = on ? 'Live' : 'Offline';
  }

  function updateClock() {
    $('clock').textContent = new Date().toLocaleTimeString([], {
      hour: '2-digit', minute: '2-digit', second: '2-digit'
    });
  }

  // ---------------- LIVE DATA ----------------

  function loadLive() {
    fetch('/api/live', { cache: 'no-store' })
      .then(function (r) {
        if (!r.ok) throw new Error('http');
        return r.json();
      })
      .then(function (d) {
        setOnline(true);
        renderLive(d);
      })
      .catch(function () {
        setOnline(false);
      });
  }

  function renderLive(d) {
    state.live = d;

    var ok = d.sensor === true && typeof d.percent === 'number' && d.percent >= 0;
    var pct = ok ? Math.max(0, Math.min(100, d.percent)) : 0;

    var rgb = COLORS.normal;
    if (!ok) rgb = COLORS.bad;
    else if (pct >= d.high) rgb = COLORS.bad;
    else if (pct < 20) rgb = COLORS.warn;
    else if (pct >= 60) rgb = COLORS.good;
    setLevelColor(rgb);

    $('bigPercent').innerHTML = ok ? pct + '<span>%</span>' : '--<span>%</span>';
    $('water').style.height = pct + '%';
    $('distance').textContent = ok ? d.cm.toFixed(1) + ' cm' : '--';
    $('updated').textContent = new Date().toLocaleTimeString([], {
      hour: '2-digit', minute: '2-digit', second: '2-digit'
    });
    $('heroHistory').textContent = (d.history || 0).toLocaleString() + ' pts';

    var sub = 'Waiting for the first reading...';
    if (ok) {
      if (d.percent >= d.high) sub = 'Above the ' + d.high + '% safety threshold';
      else if (pct < 20) sub = 'Tank is running low';
      else sub = 'Tank level is within the normal range';
    } else if (d.sensor === false) {
      sub = 'Sensor not responding - check the HC-SR04 wiring';
    }
    $('levelText').textContent = sub;

    var ac = $('alertChip');
    ac.className = 'chip ' + (!ok ? 'warn' : (d.percent >= d.high ? 'bad' : 'ok'));
    $('alertText').textContent = !ok ? 'Sensor error' : (d.percent >= d.high ? 'High level alert' : 'Level normal');

    var sc = $('sensorChip');
    sc.className = 'chip ' + (ok ? 'ok' : 'bad');
    $('sensorText').textContent = ok ? 'Sensor OK' : 'Sensor error';

    var sv = $('stSensor');
    sv.textContent = ok ? 'OK' : 'Error';
    sv.className = 'stat-value ' + (ok ? 'ok' : 'bad');
    $('stSensorSub').textContent = ok ? 'HC-SR04 responding' : 'check HC-SR04';

    $('stSamples').textContent = (typeof d.samples === 'number') ? d.samples : '--';

    var wv = $('stWifi');
    wv.textContent = d.wifi ? d.rssi + ' dBm' : 'Offline';
    wv.className = 'stat-value ' + (d.wifi ? (d.rssi > -70 ? 'ok' : '') : 'bad');
    $('stWifiSub').textContent = (d.ip && d.ip !== '0.0.0.0') ? d.ip : 'no address';

    var bv = $('stBlynk');
    bv.textContent = d.blynk ? 'Online' : 'Offline';
    bv.className = 'stat-value ' + (d.blynk ? 'ok' : '');

    $('stUptime').textContent = fmtUptime(d.uptime);
    $('stHistory').textContent = (d.history || 0).toLocaleString();
    $('stHistorySub').textContent = 'of ' + (d.capacity || 0).toLocaleString() + ' points';
    $('footerInfo').textContent = 'AquaLevel - ' + (d.ip || 'ESP32') + ' - ' + (d.timeValid ? 'clock synced' : 'clock not synced');

    if (state.hover < 0) drawChart();
  }

  // ---------------- HISTORY ----------------

  function subtitleFor(hours) {
    if (hours === 1) return 'Last hour';
    if (hours === 6) return 'Last 6 hours';
    if (hours === 168) return 'Last 7 days';
    return 'Last 24 hours';
  }

  function loadHistory() {
    fetch('/api/history?hours=' + state.hours + '&points=600', { cache: 'no-store' })
      .then(function (r) {
        if (!r.ok) throw new Error('http');
        return r.json();
      })
      .then(function (d) {
        state.points = Array.isArray(d.points) ? d.points : [];
        state.synthetic = !!d.synthetic;
        state.nowMinute = d.nowMinute || 0;
        state.hover = -1;
        $('tooltip').style.display = 'none';
        $('chartSub').textContent = subtitleFor(state.hours);
        drawChart();
        renderSummary();
      })
      .catch(function () {});
  }

  function formatAxis(t) {
    if (state.synthetic) {
      var mins = Math.max(0, Math.round(state.nowMinute - t / 60));
      if (mins < 1) return 'now';
      if (mins < 60) return mins + 'm ago';
      if (mins < 1440) return Math.round(mins / 60) + 'h ago';
      return Math.round(mins / 1440) + 'd ago';
    }
    var d = new Date(t * 1000);
    if (state.hours <= 24) {
      return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    }
    return d.toLocaleDateString([], { month: 'short', day: 'numeric' });
  }

  function tooltipTime(t) {
    if (state.synthetic) return formatAxis(t);
    var d = new Date(t * 1000);
    return d.toLocaleString([], {
      month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit'
    });
  }

  // ---------------- CHART ----------------

  function drawChart() {
    var canvas = $('chart');
    var ctx = canvas.getContext('2d');
    var rect = canvas.parentElement.getBoundingClientRect();
    var dpr = Math.min(window.devicePixelRatio || 1, 2);
    var W = Math.max(320, Math.floor(rect.width));
    var H = Math.max(180, Math.floor(rect.height));

    canvas.width = Math.floor(W * dpr);
    canvas.height = Math.floor(H * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, W, H);

    var padL = 44, padR = 18, padT = 18, padB = 30;
    var plotW = W - padL - padR;
    var plotH = H - padT - padB;

    var pts = state.points;

    if (!pts.length) {
      $('chartEmpty').style.display = 'grid';
      state.layout = null;
      return;
    }
    $('chartEmpty').style.display = 'none';

    var t0 = pts[0][0];
    var t1 = pts[pts.length - 1][0];
    var span = Math.max(60, t1 - t0);

    var xOf = function (t) { return padL + ((t - t0) / span) * plotW; };
    var yOf = function (v) {
      return padT + (1 - Math.max(0, Math.min(100, v)) / 100) * plotH;
    };

    state.layout = { t0: t0, t1: t1, span: span, padL: padL, plotW: plotW, xOf: xOf };

    ctx.lineWidth = 1;
    ctx.font = '11px ui-sans-serif, system-ui, -apple-system, "Segoe UI", sans-serif';
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';

    for (var v = 0; v <= 100; v += 25) {
      var yy = yOf(v);
      ctx.strokeStyle = 'rgba(125,211,252,0.10)';
      ctx.beginPath();
      ctx.moveTo(padL, yy);
      ctx.lineTo(W - padR, yy);
      ctx.stroke();
      ctx.fillStyle = 'rgba(127,155,179,0.85)';
      ctx.fillText(v + '%', padL - 10, yy);
    }

    var xTicks = W < 520 ? 3 : 5;
    ctx.textBaseline = 'top';
    ctx.fillStyle = 'rgba(127,155,179,0.8)';

    for (var i = 0; i <= xTicks; i++) {
      var t = t0 + (span * i) / xTicks;
      var xx = xOf(t);
      if (i === 0) { ctx.textAlign = 'left'; xx = padL; }
      else if (i === xTicks) { ctx.textAlign = 'right'; xx = W - padR; }
      else { ctx.textAlign = 'center'; }
      ctx.fillText(formatAxis(t), xx, H - padB + 10);
    }

    // Min/max band
    ctx.beginPath();
    for (var a = 0; a < pts.length; a++) {
      var xa = xOf(pts[a][0]), ya = yOf(pts[a][3]);
      if (a === 0) ctx.moveTo(xa, ya); else ctx.lineTo(xa, ya);
    }
    for (var b = pts.length - 1; b >= 0; b--) {
      ctx.lineTo(xOf(pts[b][0]), yOf(pts[b][2]));
    }
    ctx.closePath();
    ctx.fillStyle = 'rgba(56,189,248,0.07)';
    ctx.fill();

    // Area under the average line
    var grad = ctx.createLinearGradient(0, padT, 0, padT + plotH);
    grad.addColorStop(0, 'rgba(56,189,248,0.30)');
    grad.addColorStop(1, 'rgba(56,189,248,0)');

    ctx.beginPath();
    ctx.moveTo(xOf(pts[0][0]), yOf(pts[0][1]));
    for (var c = 1; c < pts.length; c++) {
      ctx.lineTo(xOf(pts[c][0]), yOf(pts[c][1]));
    }
    ctx.lineTo(xOf(pts[pts.length - 1][0]), padT + plotH);
    ctx.lineTo(xOf(pts[0][0]), padT + plotH);
    ctx.closePath();
    ctx.fillStyle = grad;
    ctx.fill();

    // Average line
    ctx.beginPath();
    for (var e = 0; e < pts.length; e++) {
      var xe = xOf(pts[e][0]), ye = yOf(pts[e][1]);
      if (e === 0) ctx.moveTo(xe, ye); else ctx.lineTo(xe, ye);
    }
    ctx.strokeStyle = '#38bdf8';
    ctx.lineWidth = 2.2;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';
    ctx.shadowColor = 'rgba(56,189,248,0.55)';
    ctx.shadowBlur = 10;
    ctx.stroke();
    ctx.shadowBlur = 0;

    // High-level threshold
    var high = (state.live && typeof state.live.high === 'number') ? state.live.high : 80;
    var hy = yOf(high);

    ctx.save();
    ctx.setLineDash([6, 6]);
    ctx.strokeStyle = 'rgba(251,191,36,0.5)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(padL, hy);
    ctx.lineTo(W - padR, hy);
    ctx.stroke();
    ctx.restore();

    ctx.font = '10px ui-sans-serif, system-ui, sans-serif';
    ctx.fillStyle = 'rgba(251,191,36,0.85)';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'bottom';
    ctx.fillText('HIGH ' + high + '%', padL + 6, hy - 4);

    // Hover marker
    if (state.hover >= 0 && state.hover < pts.length) {
      var hp = pts[state.hover];
      var hx = xOf(hp[0]);
      var hyy = yOf(hp[1]);

      ctx.save();
      ctx.setLineDash([3, 4]);
      ctx.strokeStyle = 'rgba(232,244,255,0.28)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(hx, padT);
      ctx.lineTo(hx, padT + plotH);
      ctx.stroke();
      ctx.restore();

      ctx.beginPath();
      ctx.arc(hx, hyy, 5, 0, Math.PI * 2);
      ctx.fillStyle = '#e9f5ff';
      ctx.fill();
      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2.5;
      ctx.stroke();
    }
  }

  function showTooltip(i) {
    var p = state.points[i];
    var L = state.layout;
    if (!p || !L) return;

    var tip = $('tooltip');
    var xpx = L.xOf(p[0]);
    var W = $('chart').parentElement.getBoundingClientRect().width;

    tip.innerHTML =
      '<div class="tip-time">' + tooltipTime(p[0]) + '</div>' +
      '<div class="tip-row"><span>Level</span><b>' + p[1] + '%</b></div>' +
      '<div class="tip-row"><span>Range</span><b>' + p[2] + '-' + p[3] + '%</b></div>' +
      '<div class="tip-row"><span>Distance</span><b>' + Number(p[4]).toFixed(1) + ' cm</b></div>';

    tip.style.display = 'block';

    var tw = tip.offsetWidth || 160;
    var left = Math.max(8, Math.min(xpx - tw / 2, W - tw - 8));
    tip.style.left = left + 'px';
    tip.style.top = '10px';
  }

  function handleMove(ev) {
    var L = state.layout;
    if (!L || !state.points.length) return;

    var rect = $('chart').getBoundingClientRect();
    var mx = ev.clientX - rect.left;
    var ratio = (mx - L.padL) / (L.plotW || 1);
    var t = L.t0 + ratio * L.span;

    var best = 0;
    var bd = Infinity;

    for (var i = 0; i < state.points.length; i++) {
      var dd = Math.abs(state.points[i][0] - t);
      if (dd < bd) { bd = dd; best = i; }
    }

    state.hover = best;
    showTooltip(best);
    drawChart();
  }

  function clearHover() {
    state.hover = -1;
    $('tooltip').style.display = 'none';
    drawChart();
  }

  // ---------------- SUMMARY ----------------

  function renderSummary() {
    var pts = state.points;

    if (!pts.length) {
      $('sMin').textContent = '--';
      $('sAvg').textContent = '--';
      $('sMax').textContent = '--';
      $('sTrend').textContent = '--';
      $('sTrend').className = '';
      return;
    }

    var mn = Infinity, mx = -Infinity, sum = 0;

    for (var i = 0; i < pts.length; i++) {
      var v = pts[i][1];
      if (v < mn) mn = v;
      if (v > mx) mx = v;
      sum += v;
    }

    $('sMin').textContent = Math.round(mn) + '%';
    $('sAvg').textContent = Math.round(sum / pts.length) + '%';
    $('sMax').textContent = Math.round(mx) + '%';

    var trend = pts[pts.length - 1][1] - pts[0][1];
    var el = $('sTrend');

    if (Math.abs(trend) < 1) {
      el.textContent = 'Stable';
      el.className = '';
    } else {
      el.textContent = (trend > 0 ? '+ ' : '- ') + Math.abs(Math.round(trend)) + '%';
      el.className = trend > 0 ? 'up' : 'down';
    }
  }

  // ---------------- EVENTS ----------------

  var tabs = document.querySelectorAll('.tabs button');

  for (var i = 0; i < tabs.length; i++) {
    tabs[i].addEventListener('click', function () {
      for (var j = 0; j < tabs.length; j++) tabs[j].classList.remove('active');
      this.classList.add('active');
      state.hours = parseInt(this.getAttribute('data-hours'), 10);
      loadHistory();
    });
  }

  $('clearBtn').addEventListener('click', function () {
    if (!confirm('Clear all saved water-level history?')) return;

    fetch('/api/clear', { method: 'POST' })
      .then(function () {
        state.points = [];
        state.hover = -1;
        $('tooltip').style.display = 'none';
        drawChart();
        renderSummary();
        loadHistory();
        loadLive();
      })
      .catch(function () {});
  });

  var chart = $('chart');
  chart.addEventListener('pointermove', handleMove);
  chart.addEventListener('pointerleave', clearHover);

  if (window.ResizeObserver) {
    new ResizeObserver(function () { drawChart(); }).observe(chart.parentElement);
  } else {
    window.addEventListener('resize', drawChart);
  }

  // ---------------- START ----------------

  updateClock();
  loadLive();
  loadHistory();

  setInterval(updateClock, 1000);
  setInterval(loadLive, 1000);
  setInterval(loadHistory, 60000);
})();
</script>
</body>
</html>


)WLHTML";