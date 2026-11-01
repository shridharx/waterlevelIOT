# AquaSense — Smart IoT Water Tank Monitoring System

Real-time water tank monitoring with an **ESP32 + HC-SR04** sensor, a **Node.js /
Socket.IO** backend, and a **public web dashboard** you can open from any phone
or laptop. No Blynk, no database, no app to install.

```
┌────────────┐   HC-SR04    ┌──────────────┐   HTTPS POST    ┌────────────────────┐
│  Water     │─────────────▶│    ESP32     │────────────────▶│  Node.js backend   │
│  tank      │  distance    │  filtering   │  x-api-key      │  Express+Socket.IO │
└────────────┘              └──────────────┘                 └─────────┬──────────┘
                                                                      │ WebSocket
                                                            ┌─────────▼──────────┐
                                                            │  Web dashboard     │
                                                            │  phone / laptop    │
                                                            └────────────────────┘
```

- **ESP32 → cloud**: reads 5 ultrasonic pings every 2 seconds, keeps the median,
  smooths it, converts distance to cm and %, POSTs JSON over TLS.
- **Cloud**: validates the API key and the numbers, recomputes the level, keeps the
  newest reading in memory, broadcasts it to every open dashboard.
- **Dashboard**: animated circular tank gauge, status band (EMPTY/LOW/MEDIUM/HIGH/
  FULL), sensor tiles, live Chart.js graph, device/Wi-Fi diagnostics, dark/light
  theme, responsive layout. Browser-side robustness: Socket.IO with automatic
  reconnect and REST polling fallback.
- **Alerts** (optional): a free Telegram message when the tank goes LOW or becomes
  FULL, with hysteresis and a cooldown so the phone is not spammed.

> Everything in this repository is source code: the firmware has not been flashed
> onto physical hardware and the service has not been deployed by the author.
> Test results you see in the report document describe the API/dashboard flow
> verified locally; hardware and deployment steps are procedures to follow.

---

## Table of contents

1. [Folder structure](#1-folder-structure)
2. [Hardware and wiring](#2-hardware-and-wiring)
3. [Run the backend locally (Windows)](#3-run-the-backend-locally-windows)
4. [Configure and flash the ESP32](#4-configure-and-flash-the-esp32)
5. [Sensor calibration](#5-sensor-calibration)
6. [API reference](#6-api-reference)
7. [Socket.IO events](#7-socketio-events)
8. [LOW / FULL alerts (optional)](#8-low--full-alerts-optional)
9. [Deploy to Render (public URL)](#9-deploy-to-render-public-url)
10. [Upload the project to GitHub](#10-upload-the-project-to-github)
11. [Testing procedures](#11-testing-procedures)
12. [Troubleshooting](#12-troubleshooting)
13. [Security notes](#13-security-notes)
14. [College documentation](#14-college-documentation)
15. [Legacy local-only dashboard](#15-legacy-local-only-dashboard)

---

## 1. Folder structure

```
smart-water-monitoring/
├── firmware/
│   └── water_monitor/
│       └── water_monitor.ino      ESP32 sketch (folder name must match the sketch)
├── backend/
│   ├── server.js                  Express + Socket.IO + REST API
│   ├── lib/alerts.js              Telegram LOW/FULL notifier
│   ├── package.json
│   ├── .env.example               environment variable template
│   ├── .gitignore
│   ├── public/
│   │   ├── index.html             dashboard markup
│   │   ├── css/style.css          theme + responsive layout
│   │   └── js/dashboard.js        Socket.IO client, gauge, Chart.js
│   └── tools/
│       └── simulate.js            fake sensor for testing without hardware
├── docs/
│   ├── PROJECT_REPORT.md          full MCA project documentation
│   ├── VIVA.md                    viva questions and answers
│   └── PRESENTATION_AND_DEMO.md   slide outline + faculty demo script
├── legacy/local-dashboard/        previous ESP32-hosted dashboard (kept for reference)
├── render.yaml                    Render blueprint
└── README.md
```

---

## 2. Hardware and wiring

| Component | Qty | Notes |
| --- | --- | --- |
| ESP32 DevKit V1 | 1 | 30-pin or 38-pin both work |
| HC-SR04 ultrasonic sensor | 1 | 5 V sensor, 2–400 cm range, 3 cm blind zone |
| 1 kΩ resistor | 1 | ECHO voltage divider |
| 2 kΩ resistor | 1 | ECHO voltage divider |
| Breadboard + jumper wires | — | |
| USB cable | 1 | data cable, not charge-only |
| Water tank | 1 | any open-top tank / bottle |

| HC-SR04 | ESP32 DevKit V1 | Why |
| --- | --- | --- |
| VCC | VIN (5 V) | HC-SR04 needs 5 V for stable echoes |
| GND | GND | common ground |
| TRIG | GPIO 5 | 3.3 V trigger input is fine |
| ECHO | GPIO 18 **through the divider** | ECHO outputs 5 V, GPIOs tolerate 3.3 V |

Voltage divider (protects the ESP32):

```
ECHO ──[ 1 kΩ ]──┬── GPIO 18
                 │
              [ 2 kΩ ]
                 │
                GND
```

`V_GPIO = 5 V × 2 / (1 + 2) = 3.33 V`

Power the sensor from **VIN/5 V**, not 3V3. A weak supply is the most common cause
of zero readings and random ghost echoes.

---

## 3. Run the backend locally (Windows)

Commands are for the **VS Code terminal (PowerShell)**. In Git Bash / CMD use
`cp` instead of `Copy-Item`.

```powershell
cd backend
Copy-Item .env.example .env
npm install
```

Generate a strong API key and paste it into `.env` as `DEVICE_API_KEY` (keep it
for the firmware step too):

```powershell
node -e "console.log(require('crypto').randomBytes(32).toString('hex'))"
```

Start the server:

```powershell
npm run dev          # or: npm start
```

Open <http://localhost:3000>. Until the ESP32 (or the simulator) sends a reading
the dashboard shows `—` and `ESP32 offline`.

Test the pipeline without hardware — second terminal:

```powershell
cd backend
npm run simulate
```

You should see `200 {"ok":true,...}` every 2 seconds and the gauge/graph moving
in the browser.

### Environment variables (`backend/.env`)

| Variable | Default | Meaning |
| --- | --- | --- |
| `PORT` | `3000` | HTTP port. Render injects its own value — never hard-code it. |
| `DEVICE_API_KEY` | — | **Required.** Shared secret the ESP32 must send. The server refuses to start without it. |
| `DEVICE_ID` | empty | Optional. If set, readings from any other `deviceId` are rejected with 400. |
| `OFFLINE_AFTER_MS` | `15000` | No reading for this long → dashboard shows the device offline. |
| `CORS_ORIGIN` | `*` | Comma-separated allowed browser origins. The dashboard is served by the same server, so `*` is acceptable; tighten it in production if you embed the page elsewhere. |
| `TELEGRAM_BOT_TOKEN` | empty | Optional. Bot token from @BotFather; alerts stay off while empty. |
| `TELEGRAM_CHAT_ID` | empty | Optional. Chat that receives the alerts. |
| `ALERT_LOW_PERCENT` | `25` | Send a LOW alert below this level. |
| `ALERT_FULL_PERCENT` | `90` | Send a FULL alert at or above this level. |
| `ALERT_HYSTERESIS_PERCENT` | `2` | Threshold margin that prevents alert flapping. |
| `ALERT_COOLDOWN_MS` | `600000` | Minimum gap between two alert messages (10 min). |

---

## 4. Configure and flash the ESP32

### 4.1 Arduino IDE setup

1. Install the **Arduino IDE 2.x**.
2. **File → Preferences → Additional boards manager URLs**:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. **Boards Manager** → install **esp32 by Espressif Systems** (3.x).
4. **Board**: *ESP32 Dev Module*, **Upload speed**: 921600, **Partition scheme**:
   *Default 4MB with spiffs*.
5. No extra libraries — `WiFi`, `WiFiClientSecure` and `HTTPClient` ship with the core.
6. Open **`firmware/water_monitor/water_monitor.ino`** in the IDE. The `.ino`
   must stay inside a folder with the same name — that is why the folder exists.
   Do not open `legacy/local-dashboard/WaterLabelMonitor.ino`: that is the old
   Blynk/local-server sketch and it never talks to this backend.

### 4.2 Values to change in `firmware/water_monitor/water_monitor.ino`

Every one is marked `CHANGE ME`:

| Constant | Example | What it is |
| --- | --- | --- |
| `WIFI_SSID` | `"MyHomeWiFi"` | 2.4 GHz network name (ESP32 cannot use 5 GHz) |
| `WIFI_PASSWORD` | `"secret"` | Wi-Fi password |
| `BACKEND_HOST` | `"aquasense.onrender.com"` | Render host **without** `https://` |
| `BACKEND_PORT` | `443` | 443 for HTTPS, 3000 for a local server |
| `USE_HTTPS` | `1` | `1` on Render, `0` only for a LAN server |
| `DEVICE_API_KEY` | `"9f3c…"` | Same value as `DEVICE_API_KEY` on the server |
| `DEVICE_ID` | `"esp32-water-01"` | Must match `DEVICE_ID` if that is set server-side |
| `TANK_HEIGHT_CM` | `19.0` | Water depth that equals 100 % |
| `EMPTY_DISTANCE_CM` | `19.0` | Distance reading when the tank is empty |

Leave the default `BACKEND_HOST` and the Wi-Fi placeholders until you have a
deployed URL — the sketch compiles without them but connects to nothing.

### 4.3 Upload

1. Connect the ESP32, select the correct **COM port**.
2. Click **Upload**. If upload fails, hold **BOOT** while it says `Connecting…`.
3. Open **Serial Monitor** at **115200 baud**. Expected output:

```
==========================================================
 Smart Water Tank Monitor - ESP32
==========================================================
Device       : esp32-water-01 (fw 1.0.0)
Backend      : https://aquasense.onrender.com:443/api/water-level
Tank height  : 19.0 cm (empty distance 19.0 cm)
Sample cycle : 2000 ms
TLS          : enabled, certificate validation ON
[WiFi] connecting to MyHomeWiFi
[WiFi] got IP: 192.168.1.42
[Sensor] 12.4 cm | water 6.6/19.0 cm | 34.7% | samples 5/5 | RSSI -58
[HTTP] 200 {"ok":true,"accepted":true,...}
```

### 4.4 Confirm the correct firmware is running

The Serial Monitor at 115200 should look like this:

```
[WiFi] MAC: 24:6F:28:xx:xx:xx
Device       : esp32-water-01 (fw 1.0.0)
Backend      : http://192.168.0.182:3000/api/water-level
Tank height  : 19.0 cm (empty distance 19.0 cm)
[WiFi] got IP: 192.168.0.55
[Sensor] 12.4 cm | water 6.6/19.0 cm | 34.7% | samples 5/5 | RSSI -58
[HTTP] 200 {"ok":true,"accepted":true,...}
```

How to read the output:

| What you see | Meaning |
| --- | --- |
| `[Sensor]` + `[HTTP] 200` | correct firmware, posting to the backend |
| `Blynk:down`, `Distance: …` without brackets | the **legacy** sketch is running — re-upload `firmware/water_monitor/water_monitor.ino` |
| `[CONFIG] … is still the placeholder` | fill in the SSID / key / host constants |
| `[WiFi] reason 201 NO_AP_FOUND` | SSID not visible: use a 2.4 GHz network (the sketch lists what it can see) |
| `[WiFi] "…" is visible - check WIFI_PASSWORD` | name is right, password is wrong |
| `[HTTP] 401` | `DEVICE_API_KEY` differs from the server's `.env`/Render value |
| `[HTTP] POST failed` | backend not running, wrong `BACKEND_HOST`/`BACKEND_PORT`, or `USE_HTTPS` mismatch |

When the ESP32 is the source of the data, the dashboard's **Firmware** row shows
`1.0.0` instead of `sim-1.0.0`, and the raw distance matches the Serial Monitor.

---

## 5. Sensor calibration

Mount the sensor **flat and perpendicular** at the top of the tank, facing the
water, at least 3 cm above the highest water level.

1. Empty the tank and note the distance the sensor reports, e.g. `20.4 cm`.
   This is `EMPTY_DISTANCE_CM` (the 0 % reference).
2. Decide how much water depth means 100 %. It is normally the distance from the
   sensor to the bottom, but the HC-SR04 blind zone (≈2–3 cm) means the top of
   the tank cannot be measured, so pick a value that makes the top of the
   *measurable* range read 100 %. Example: empty distance 20.4 cm, water hits the
   blind zone at 2.6 cm → `TANK_HEIGHT_CM = 20.4 − 2.6 = 17.8`.
3. Set both values:

```cpp
#define TANK_HEIGHT_CM     17.8f   // water depth that means 100 %
#define EMPTY_DISTANCE_CM  20.4f   // measured distance when empty (0 %)
```

Any reading closer than 2 cm is treated as a sensor error (`MIN_VALID_CM`), and
so is anything beyond `EMPTY_DISTANCE_CM + 8 cm` (ghost echo).

Math used by the firmware (`waterHeightCm()` / `percentFor()`):

```
water height (cm) = clamp(EMPTY_DISTANCE_CM − measured distance, 0, TANK_HEIGHT_CM)
percentage (%)    = clamp(water height / TANK_HEIGHT_CM × 100, 0, 100)
```

The backend repeats the same calculation from `distanceCm` + `tankHeightCm` +
`emptyDistanceCm`, so a firmware arithmetic bug can never publish > 100 %.

**Filtering** (both in `readFilteredCm()`): five pings per cycle, echoes outside
2 cm…(empty distance + 8 cm) are discarded, the median of the remaining samples
is used, and an exponential moving average (`SMOOTHING_ALPHA = 0.35`) removes
single-sample jumps. Fewer than 3 valid echoes → the reading is sent with
`"valid": false`, the dashboard shows a sensor warning and keeps the last good
level.

---

## 6. API reference

Base URL: `http://localhost:3000` or `https://<your-service>.onrender.com`.

All endpoints return JSON. The ingestion endpoint requires the device key in the
`x-api-key` header (or `Authorization: Bearer <key>`).

### `POST /api/water-level` — ESP32 → server

```bash
curl -X POST https://aquasense.onrender.com/api/water-level ^
  -H "Content-Type: application/json" ^
  -H "x-api-key: YOUR_DEVICE_API_KEY" ^
  -d "{\"deviceId\":\"esp32-water-01\",\"valid\":true,\"distanceCm\":13.2,\"tankHeightCm\":19,\"emptyDistanceCm\":19,\"samples\":5,\"rssi\":-58,\"uptimeSec\":120,\"firmware\":\"1.0.0\"}"
```

| Field | Type | Required | Validation |
| --- | --- | --- | --- |
| `deviceId` | string | yes | `[A-Za-z0-9._-]{1,64}`, must match `DEVICE_ID` when set |
| `valid` | boolean | no | `false` = sensor error, carries no level |
| `distanceCm` | number | yes when valid | 0–500 |
| `tankHeightCm` | number | yes when valid | 0–500 |
| `emptyDistanceCm` | number | no | 0–500, defaults to `tankHeightCm` |
| `samples`, `rssi`, `uptimeSec`, `firmware` | number/string | no | stored for diagnostics |

Responses:

```
200 { "ok": true, "accepted": true, "receivedAt": "2026-11-01T09:15:02.113Z", "percent": 30.5 }
202 { "ok": true, "accepted": false, "reason": "sensor-error", ... }   // valid:false
400 { "ok": false, "error": "distanceCm must be a number between 0 and 500" }
401 { "ok": false, "error": "Invalid or missing device API key" }
```

### `GET /api/water-level` — latest reading

```json
{
  "ok": true,
  "data": {
    "deviceId": "esp32-water-01",
    "valid": true,
    "distanceCm": 13.2,
    "waterHeightCm": 5.8,
    "percent": 30.5,
    "tankHeightCm": 19,
    "emptyDistanceCm": 19,
    "samples": 5,
    "rssi": -58,
    "uptimeSec": 120,
    "firmware": "1.0.0",
    "receivedAt": "2026-11-01T09:15:02.113Z"
  },
  "device": { "online": true, "lastSeenAt": "2026-11-01T09:15:02.113Z", "readings": 61, "offlineAfterMs": 15000 }
}
```

Before the first reading it returns `200` with `"data": null`.

### `GET /api/status` — device + server health

```json
{
  "ok": true,
  "server": { "version": "1.0.0", "uptimeSec": 812, "now": "…", "sockets": 2, "database": false },
  "device": { "online": true, "deviceId": "esp32-water-01", "lastSeenAt": "…", "sensorOk": true,
              "readings": 61, "offlineAfterMs": 15000 }
}
```

### `GET /api/health` — liveness probe (used by Render)

```json
{ "ok": true, "status": "healthy", "uptimeSec": 812, "now": "…" }
```

---

## 7. Socket.IO events

The browser connects to the same origin; no URL configuration is needed.

| Event | Direction | Payload |
| --- | --- | --- |
| `meta` | server → client | `{ serverVersion, offlineAfterMs, serverTime }` |
| `water:update` | server → client | the `data` object of `GET /api/water-level` |
| `device:status` | server → client | `{ online, deviceId, lastSeenAt, sensorOk, readings, offlineAfterMs }` |
| `sensor:error` | server → client | `{ deviceId, at, message }` |

On every new connection the server pushes `meta`, `device:status` and (if a
reading exists) `water:update`, so a late-joining phone is instantly up to date.

---

## 8. LOW / FULL alerts (optional)

The backend can send a free Telegram message when the tank runs LOW or becomes
FULL. Alerts are optional: with no credentials the server behaves exactly as
before, and a Telegram failure never affects ingestion (the ESP32 still gets 200).

**Rules**

- LOW when the level drops below `ALERT_LOW_PERCENT` (25 %).
- FULL when the level reaches `ALERT_FULL_PERCENT` (90 %).
- Hysteresis (2 %): leaving LOW needs ≥ 27 %, leaving FULL needs ≤ 88 %, so a
  level sitting on a threshold cannot flap.
- One message per transition, not per reading, plus a 10-minute cooldown
  (`ALERT_COOLDOWN_MS`).
- Thresholds, current zone and send count are exposed by `GET /api/status`.

**Setup (5 minutes)**

1. In Telegram, open **@BotFather** → `/newbot` → choose a name → copy the token
   (`123456789:AA…`).
2. Send any message (e.g. `/start`) to your new bot.
3. Open `https://api.telegram.org/bot<YOUR_TOKEN>/getUpdates` and copy
   `result[0].message.chat.id` (a negative number for groups).
4. Put both values in `backend/.env` (or Render → Environment):

```ini
TELEGRAM_BOT_TOKEN=123456789:AA...
TELEGRAM_CHAT_ID=123456789
```

5. Restart the backend. The startup log confirms it:

```
  alerts: Telegram enabled (LOW < 25%, FULL >= 90%)
```

Alert message:

```
AquaSense alert: LOW WATER LEVEL — refill soon

Level: 18.4%
Water height: 3.5 cm of 19.0 cm
Distance from sensor: 15.5 cm
Device: esp32-water-01
Time: 2026-11-01T09:20:11.402Z
```

**Testing** — run `npm run simulate`: the simulator sweeps 5 % → 98 %, so it
crosses LOW and then FULL within a minute or two. The first crossing sends a
message; repeats are blocked by hysteresis/cooldown. Without credentials you can
still inspect the notifier state:

```powershell
curl http://localhost:3000/api/status
```

```json
"alerts": { "enabled": true, "channel": "telegram", "lowPercent": 25, "fullPercent": 90,
            "zone": "low", "sentCount": 1, "lastSentAt": "…" }
```

Troubleshooting: `getUpdates` returns an empty `result` → send `/start` to the bot
first; `401 Unauthorized` in the bot reply → wrong token; `chat not found` → wrong
`chat_id` or you never messaged the bot.

---

## 9. Deploy to Render (public URL)

### Option A — Blueprint (uses `render.yaml`)

1. Push the repository to GitHub (section 10).
2. Render dashboard → **New +** → **Blueprint** → select the repository.
3. Render reads `render.yaml`, shows one web service (`smart-water-monitoring`),
   and generates a random `DEVICE_API_KEY`. Click **Apply**.
4. Wait for the deploy log to say `listening on port …`.
5. Open the service → **Environment** → copy `DEVICE_API_KEY`, then paste the same
   value into `firmware/water_monitor/water_monitor.ino` and re-upload the sketch.
6. Public URL: `https://<service-name>.onrender.com` — shown at the top of the
   service page and under **Settings → Public URL**.

### Option B — Manual web service

1. **New +** → **Web Service** → connect the repository.
2. **Root Directory**: `backend`
3. **Runtime**: Node
4. **Build Command**: `npm install`
5. **Start Command**: `npm start`
6. **Instance type**: Free
7. **Environment variables**: `DEVICE_API_KEY` (long random string),
   optional `DEVICE_ID`, `OFFLINE_AFTER_MS=15000`, `CORS_ORIGIN=*`.
   Do **not** set `PORT` — Render provides it and `server.js` reads `process.env.PORT`.
8. **Health Check Path**: `/api/health` (optional but recommended).
9. Click **Create Web Service**.

### Verify the deployment

```powershell
curl https://<your-service>.onrender.com/api/health
curl https://<your-service>.onrender.com/api/status
```

Then update `BACKEND_HOST`, `BACKEND_PORT 443`, `USE_HTTPS 1` and
`DEVICE_API_KEY` in the firmware, upload, and the public dashboard fills in.

### Render notes

- **Free plan sleeps** after ~15 minutes without inbound traffic. The first request
  after that can take 30–60 s; the firmware retries with exponential backoff, so it
  recovers automatically. Continuous 2-second uploads keep the service awake.
- The free disk is ephemeral — that is fine here, no data is persisted by design.
- If your certificate is not issued by Let's Encrypt, see
  [Certificate troubleshooting](#certificate-troubleshooting).

---

## 10. Upload the project to GitHub

```powershell
git init
git add .
git commit -m "Smart IoT water tank monitoring with ESP32 and live dashboard"
git branch -M main
git remote add origin https://github.com/<your-username>/smart-water-monitoring.git
git push -u origin main
```

**Before pushing**, make sure:

- `backend/.env` does not exist in the commit (`backend/.gitignore` already
  excludes it — only `.env.example` is tracked).
- `firmware/water_monitor/water_monitor.ino` does not contain your real Wi-Fi
  password or API
  key. Either keep the placeholders in the public copy, or keep those two lines
  in a local `secrets.h` that you add to `.gitignore`.
- The old `legacy/local-dashboard/` sketch no longer contains credentials
  (they were removed when it was moved here).

Rotate any key that was ever committed: Render → Environment → regenerate
`DEVICE_API_KEY`, update the firmware, re-upload.

### Certificate troubleshooting

The sketch trusts **ISRG Root X1** (Let's Encrypt). Render's `*.onrender.com`
certificate is currently issued through that chain. If you get
`SSL error` / `-1` in the Serial Monitor, print the chain of your own service and
replace `ROOT_CA` with the last certificate of the chain:

```bash
openssl s_client -connect your-service.onrender.com:443 -servername your-service.onrender.com -showcerts
```

Copy the `-----BEGIN CERTIFICATE----- … -----END CERTIFICATE-----` block of the
**root** (the last one) into `ROOT_CA`. As a last resort for a classroom demo only,
set `TLS_VERIFY 0` — that disables certificate validation and should never be
used for a real installation.

---

## 11. Testing procedures

| # | Test | How | Pass criteria |
| --- | --- | --- | --- |
| 1 | Backend health | `curl http://localhost:3000/api/health` | `{"ok":true,...}` |
| 2 | Auth rejects | POST without `x-api-key` | HTTP 401 |
| 3 | Auth accepts | `npm run simulate` | HTTP 200 every 2 s |
| 4 | Validation | POST `distanceCm: 9999` | HTTP 400 with a clear message |
| 5 | Sensor error path | POST `"valid": false` | HTTP 202, dashboard shows sensor warning |
| 6 | Socket live update | open two browsers | both update without refresh |
| 7 | Offline detection | stop the simulator | badge flips to offline after ~15 s |
| 8 | Dashboard reconnect | stop/start `npm run dev` while the page is open | "Reconnecting…" then "Live", no reload |
| 9 | Wi-Fi loss | power off the router for 30 s | ESP32 reconnects, uploads resume |
| 10 | Public internet access | phone on mobile data opens the Render URL | live data, same behaviour |
| 11 | Postman | `POST {{baseUrl}}/api/water-level` with header `x-api-key` | 200 + Socket.IO broadcast |
| 12 | Hardware | Serial Monitor shows plausible cm and `HTTP 200` | dashboard matches the ruler measurement |
| 13 | Alerts | `npm run simulate` with Telegram configured | one LOW message, one FULL message, no flapping |
| 14 | Alerts disabled | remove `TELEGRAM_BOT_TOKEN` | server starts, no alerts, no errors |

---

## 12. Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| ESP32 cannot connect to Render (`HTTP -1`, `SSL error`) | Wrong host/port, TLS, or Render cold start | Check `BACKEND_HOST` has no `https://`, port 443, `USE_HTTPS 1`. Wake the service with `curl .../api/health`, then reset the ESP32. Replace the root CA if needed. |
| **HTTP 401** | API key mismatch | Compare `DEVICE_API_KEY` in `.env`/Render with the sketch — no spaces, no quotes. |
| **HTTP 400** | Field validation failed | Read the `error` text: usually `tankHeightCm` missing or `distanceCm` out of range. |
| **HTTP 500** | Server exception | Check Render logs. Restart the service; confirm `npm install` succeeded. |
| Sensor always returns 0 / `no valid echo` | 3.3 V power, loose jumper, dead sensor | Power VCC from VIN 5 V, reseat wires, check TRIG/ECHO are not swapped, test the sensor with `pulseIn` alone. |
| Distance wildly wrong / random jumps | Missing ECHO divider, reflective walls, ghost echoes | Verify 1k/2k divider, move the sensor away from corners, keep `MAX_VALID_CM` just above the tank depth. |
| Incorrect water percentage | Calibration values | Re-measure with the tank empty and recalibrate `EMPTY_DISTANCE_CM` / `TANK_HEIGHT_CM` (section 5). |
| Dashboard shows offline | No uploads for 15 s | Check Serial Monitor for `[HTTP]` errors, Wi-Fi status, and that the service is awake. |
| Socket.IO connection failure | Proxy blocking WebSockets, wrong URL | The client falls back to `transports: ['websocket','polling']` and then to 5 s REST polling automatically; check the browser console for the socket error. |
| Wi-Fi connection failed | 5 GHz network, wrong password, hotspot asleep | ESP32 is 2.4 GHz only; re-check SSID/password; watch the disconnect reason printed by the sketch. |
| Dashboard loads but everything is `—` | No reading has been accepted yet | Send one reading with `npm run simulate` and check `GET /api/water-level`. |
| Arduino IDE: `main file missing from sketch` or asks to move the file | the `.ino` is not in a folder of the same name | Open `firmware/water_monitor/water_monitor.ino` directly; keep that folder intact. |
| Serial shows `Blynk:down`, `Level: 0%`, `distance` but no `[HTTP]` lines | the board is running the **legacy** local sketch | Upload `firmware/water_monitor/water_monitor.ino` instead; the new firmware prints `[Sensor]` and `[HTTP] 200`. |
| Serial shows `[WiFi] reason 201 NO_AP_FOUND` | the SSID is not visible to the ESP32 (5 GHz-only network, hotspot off, wrong name) | Read the `[WiFi] scanning…` list printed by the firmware and use a 2.4 GHz SSID from it. |
| Dashboard numbers jump between two different values | the simulator is still posting while the ESP32 also posts | Stop `npm run simulate`; the device card shows firmware `sim-1.0.0` for simulated data. |
| No Telegram alert arrives | Alerts disabled, wrong token/chat, or still inside the cooldown | Check the startup log for `alerts: Telegram enabled` and `/api/status` → `alerts.enabled`/`sentCount`; send `/start` to the bot; check Render logs for `[alerts] Telegram error`. |

---

## 13. Security notes

- The device key lives only in `backend/.env` (or the Render environment) and in
  the firmware — never in `public/js/`.
- Reads (`GET`) are public and read-only so a phone can open the dashboard
  without a login. Only ingestion is authenticated.
- All incoming values are type-checked, range-checked and clamped; the level is
  recomputed server-side.
- `helmet`-style hardening is minimal by design (no external scripts besides the
  Chart.js CDN): `X-Content-Type-Options`, `Referrer-Policy`, JSON body limit 8 kB.
- Production traffic is HTTPS end-to-end (Render certificate); the ESP32 verifies
  it against a pinned root CA.
- Never commit `.env`, Wi-Fi credentials, the device key or the Telegram bot
  token. Rotate the key if it leaks; revoke a leaked bot token with @BotFather
  (`/revoke`).

---

## 14. College documentation

- [`docs/PROJECT_REPORT.md`](docs/PROJECT_REPORT.md) — abstract, problem
  statement, architecture, circuit, algorithm, flowchart, maths, API docs,
  testing, advantages/limitations/future scope, references.
- [`docs/VIVA.md`](docs/VIVA.md) — viva questions with answers.
- [`docs/PRESENTATION_AND_DEMO.md`](docs/PRESENTATION_AND_DEMO.md) — 12-slide
  outline, faculty demo script, expected output descriptions.

---

## 15. Legacy local-only dashboard

`legacy/local-dashboard/` is the earlier version of this project where the ESP32
served the dashboard itself over the LAN (plus an SSD1306 OLED and optional
Blynk). It still works offline but is reachable only on the same Wi-Fi and
exposes no authentication, so it is kept for reference, not recommended for the
final submission. Its Wi-Fi/Blynk credentials were replaced with placeholders
when it was moved here.
