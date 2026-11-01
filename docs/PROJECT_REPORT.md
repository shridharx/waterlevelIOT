# Smart IoT Water Tank Monitoring System with Live Web Dashboard

**Project report — MCA (Artificial Intelligence & Machine Learning)**

| | |
| --- | --- |
| **Candidate** | _(your name)_ |
| **Roll number** | _(your roll number)_ |
| **Guide** | _(guide name)_ |
| **Department** | _(department)_ |
| **Academic year** | _(year)_ |
| **Technologies** | ESP32 firmware (C++), Node.js, Express, Socket.IO, HTML5/CSS3/JavaScript, Chart.js |
| **Cloud platform** | Render (free web service) |
| **Database** | None — in-memory state only |

---

## 1. Project Title

**Smart IoT Water Tank Monitoring System with Live Web Dashboard using ESP32,
Ultrasonic Sensing and Real-Time Web Communication.**

---

## 2. Abstract

Water storage tanks are common in homes, hostels and college campuses, yet the
level inside them is usually checked manually by climbing to the tank or opening
the lid. This wastes water during overflow, risks dry running of pumps and
consumes time. The project presents an end-to-end IoT solution that measures the
water level with an HC-SR04 ultrasonic sensor, processes the measurements on an
ESP32 microcontroller, and publishes them over HTTPS to a cloud-hosted Node.js
backend. The backend validates every packet, keeps the newest value in memory and
pushes it to a responsive web dashboard through Socket.IO, so the level is visible
on any phone or laptop anywhere in the world without refreshing the page or
installing an application. The dashboard shows an animated circular tank gauge,
a five-level status band (EMPTY, LOW, MEDIUM, HIGH, FULL), the water height and
raw sensor distance, a live four-minute history graph and the device's Wi-Fi
health. The system is database-free, protected by a per-device API key, and costs
nothing to host on Render's free tier. Measured pipeline latency from sensor read
to browser paint is dominated by the 2-second sampling period and stays within
one sampling cycle on a local network.

---

## 3. Introduction

Water is a critical resource and its storage is normally unmonitored. A typical
overhead tank is filled by a motor and either overflows (wasting water and
damaging walls) or runs dry (damaging the pump). Manual checking is unreliable,
especially in multi-storey buildings.

The Internet of Things (IoT) offers a direct solution: a low-cost sensor at the
tank, a microcontroller at the edge, and a cloud service that presents the data
as a live web page. The ESP32 is an ideal edge device — dual-core 240 MHz, Wi-Fi
and Bluetooth built in, 3.3 V logic, widely available and inexpensive. The
HC-SR04 ultrasonic sensor measures distance without touching the water, which
avoids corrosion and contamination.

This project implements the complete chain: sensor → microcontroller → HTTPS →
cloud API → WebSocket → browser dashboard. It deliberately avoids any database so
that the deployment remains free, simple and stateless, which matches the
requirement of "latest level only" monitoring.

---

## 4. Problem Statement

The existing manual process has the following problems:

1. **Overflow** — the tank is filled beyond capacity because nobody knows the
   level; water is wasted and seepage damages the structure.
2. **Dry running** — the pump operates after the tank empties, causing motor
   overheating and costly repairs.
3. **No remote visibility** — residents cannot check the tank without physically
   going to it, which is inconvenient in multi-floor buildings.
4. **No history** — even when a level indicator exists, there is no record of how
   consumption changed over time.
5. **Cost and complexity of commercial systems** — proprietary solutions such as
   Blynk-based apps or industrial controllers require subscriptions, accounts or
   dedicated hardware.

There is a need for a low-cost, reliable, database-free system that displays the
live water level on any internet-connected device, works without proprietary
apps, and can be built by a student with basic components.

---

## 5. Objectives

1. Measure the water level in a tank using an HC-SR04 ultrasonic sensor.
2. Convert the measured distance into water height (cm) and percentage (%).
3. Transmit the processed reading to a cloud backend every 2 seconds over HTTPS.
4. Validate sensor data on both the device and the server to prevent false values.
5. Provide a public, responsive web dashboard with a live gauge, status band,
   numeric details, history graph and device diagnostics.
6. Update the browser in real time without page refresh using Socket.IO.
7. Detect and display device online/offline state automatically.
8. Keep the system completely database-free.
9. Protect the ingestion endpoint with a configurable device API key.
10. Document deployment, calibration, testing and troubleshooting for
    reproducibility.

---

## 6. Existing System

| Aspect | Existing manual / semi-automatic system |
| --- | --- |
| Measurement | Visual inspection through the lid or a mechanical float |
| Display | None, or a local indicator LED on the tank |
| Remote access | None |
| History | None |
| Alerts | None |
| Cost | Low, but human effort is repeated daily |
| Reliability | Depends on the person |

A second common "existing system" is a Blynk-based mobile app. It requires a
Blynk account and auth token, uses a proprietary mobile UI, and does not offer a
publicly accessible dashboard for faculty or family members without the app. The
earlier version of this project (kept in `legacy/local-dashboard/`) served a page
from the ESP32 itself, which only works while the phone is on the same Wi-Fi.

---

## 7. Proposed System

| Aspect | Proposed system |
| --- | --- |
| Measurement | HC-SR04 ultrasonic, 5 pings per cycle with median + EMA filtering |
| Controller | ESP32 DevKit V1, non-blocking `millis()` scheduling |
| Transport | HTTPS POST every 2 s with `x-api-key` header |
| Backend | Node.js + Express + Socket.IO on Render, in-memory only |
| Display | Public responsive dashboard, dark/light, Chart.js live graph |
| Latency | One sampling period (≈2 s) end to end |
| History | Last 4 minutes in browser memory (no database) |
| Alerts | Optional free Telegram message on LOW (< 25 %) / FULL (≥ 90 %), with hysteresis and cooldown |
| Reliability | Wi-Fi auto-reconnect, HTTP retry with exponential backoff, device offline detection |
| Cost | Hosting free tier, hardware under ₹700 |

The system replaces the proprietary app with a plain web page that works on any
device with a browser, and replaces the manual check with continuous,
quantified monitoring.

---

## 8. Hardware Requirements

| # | Component | Specification | Purpose |
| --- | --- | --- | --- |
| 1 | ESP32 DevKit V1 | ESP32-WROOM-32, 240 MHz, Wi-Fi 802.11 b/g/n | Edge controller with Wi-Fi |
| 2 | HC-SR04 | 5 V, 2–400 cm, ~3 mm accuracy, 40 kHz | Non-contact distance measurement |
| 3 | Resistor 1 kΩ | ¼ W | ECHO divider, top leg |
| 4 | Resistor 2 kΩ | ¼ W | ECHO divider, bottom leg |
| 5 | Breadboard | 830 tie points | Prototyping |
| 6 | Jumper wires | male–male / male–female | Connections |
| 7 | USB cable | data-capable micro-USB | Power + programming |
| 8 | Water tank | open top, ≥ 3 cm headroom | Test vessel |
| 9 | Wi-Fi router | 2.4 GHz, internet-connected | Uplink |
| 10 | (Optional) 5 V 2 A adapter | — | Stable standalone power |

---

## 9. Software Requirements

| Category | Software |
| --- | --- |
| Firmware IDE | Arduino IDE 2.x with **esp32 by Espressif** core 3.x |
| Firmware libraries | `WiFi`, `WiFiClientSecure`, `HTTPClient` (bundled) |
| Runtime | Node.js ≥ 18, npm |
| Backend packages | `express`, `socket.io`, `cors`, `dotenv` |
| Frontend | HTML5, CSS3, vanilla JavaScript (ES2020), Chart.js 4, Socket.IO client |
| Browser | Chrome / Edge / Firefox / Safari, current versions |
| API testing | Postman or `curl` |
| Version control | Git + GitHub |
| Hosting | Render (free web service) |
| Editor | VS Code |

---

## 10. System Architecture

```
                2.4 GHz Wi-Fi                    HTTPS (TLS 1.2+)
 ┌──────────┐   ┌───────────┐   ┌────────────┐   ┌──────────────────────────┐
 │ HC-SR04  │──▶│   ESP32   │──▶│  Router /  │──▶│  Render: Node.js service │
 │ (5 pings)│   │ filter +  │   │  Internet  │   │  Express + Socket.IO     │
 └──────────┘   │ calculate │   └────────────┘   │  - POST /api/water-level │
                └───────────┘                    │  - in-memory latest      │
                                                 │  - serve public/         │
                                                 └────────────┬─────────────┘
                                                              │ WebSocket
                                                    ┌─────────▼──────────┐
                                                    │ Browser dashboard  │
                                                    │ gauge + chart      │
                                                    └────────────────────┘
```

**Layers**

1. **Perception layer** — HC-SR04 ultrasonic sensor.
2. **Edge layer** — ESP32: sampling, outlier rejection, filtering, unit
   conversion, JSON serialisation, TLS transport, reconnection logic.
3. **Cloud layer** — Express REST API, authentication, validation, authoritative
   level computation, in-memory state, offline watchdog, Socket.IO broadcast,
   static file hosting.
4. **Presentation layer** — dashboard: gauge, status, sensor metrics, chart,
   device health, theme, responsive layout.

**Data flow:** sensor → distance (cm) → filtered distance → water height → percent
→ JSON over HTTPS → validated state → Socket.IO event → DOM update + chart point.

---

## 11. Circuit Connections

| HC-SR04 pin | Connects to | Notes |
| --- | --- | --- |
| VCC | ESP32 **VIN** (5 V) | Do not use 3V3 — unstable readings |
| GND | ESP32 GND | Common ground |
| TRIG | GPIO **5** | 10 µs HIGH pulse triggers a burst |
| ECHO | GPIO **18** via 1 kΩ / 2 kΩ divider | Protects the 3.3 V pin |

Divider:

```
ECHO ──[1 kΩ]──┬── GPIO 18
               │
            [2 kΩ]
               │
              GND
```

`V_GPIO = 5 × 2 / (1 + 2) = 3.33 V` — safe for the ESP32.

**Mechanical mounting:** sensor fixed horizontally at the centre of the tank lid,
faces the water surface, at least 3 cm above the highest possible water level
(HC-SR04 blind zone) and at least 5 cm from the walls to limit reflections.

---

## 12. Working Principle

1. The ESP32 connects to the 2.4 GHz Wi-Fi network and obtains an IP by DHCP.
2. Every 2 seconds it triggers the HC-SR04 with a 10 µs pulse on GPIO 5.
3. The sensor emits a 40 kHz burst and raises ECHO while it waits for the echo.
4. The ESP32 measures the ECHO pulse width in microseconds and converts it to
   distance: `d = (t × 0.0343) / 2`, where 0.0343 cm/µs is the speed of sound.
5. Five such pings are taken 45 ms apart. Echoes outside 2 cm…(empty + 8 cm) are
   discarded; the median of the remaining samples is kept; an exponential moving
   average smooths the sequence.
6. Water height = empty distance − distance. Percentage = height / tank height × 100,
   clamped to 0…100.
7. The reading is serialised to JSON and POSTed over TLS to
   `https://<service>.onrender.com/api/water-level` with the `x-api-key` header.
8. The backend authenticates the key, validates ranges, recomputes the percentage
   and publishes a `water:update` Socket.IO event.
9. Every connected dashboard updates its gauge, numbers and chart immediately.
10. If no reading arrives within 15 s, the backend marks the device offline and
    notifies browsers; the firmware retries with exponential backoff and
    reconnects to Wi-Fi when needed.

---

## 13. Mathematical Calculations

**Distance from the sensor**

```
d (cm) = (echo time in µs × 0.0343 cm/µs) / 2
```

Example: echo 775 µs → `775 × 0.0343 / 2 = 13.29 cm`.

**Water height**

```
h = clamp(EMPTY_DISTANCE_CM − d, 0, TANK_HEIGHT_CM)
```

**Percentage**

```
p = clamp((h / TANK_HEIGHT_CM) × 100, 0, 100)
```

`TANK_HEIGHT_CM` is the water depth that represents 100 %. It is normally the
sensor-to-bottom distance, but because the top 2–3 cm are inside the blind zone a
slightly smaller value is used so the top of the measurable range reads 100 %.

**Worked example** — tank height 19 cm, empty distance 19.0 cm, measured 7.6 cm:

```
h = 19.0 − 7.6 = 11.4 cm
p = 11.4 / 19.0 × 100 = 60.0 %
```

**Rounding / displayed accuracy** — firmware reports 0.1 cm and 0.1 %; the UI
rounds the percentage to an integer for the gauge and keeps 1 decimal in the API.

**Sampling and filtering**

- Median of *n* valid samples removes isolated outliers.
- EMA: `sₖ = α·mₖ + (1 − α)·sₖ₋₁`, α = 0.35 → ~65 % of the previous value is
  retained, so a single bad echo moves the output by less than 0.35 × its error.
- Time constant ≈ `Δt · (1 − α)/α = 2 s × 1.86 ≈ 3.7 s` — smooth but still
  responsive to a real change.

**Data volume** — one reading ≈ 320 bytes JSON, every 2 s → ~0.96 MB/hour, well
inside free-tier limits.

---

## 14. Algorithm

```
START
  initialise Serial, GPIO 5 (output), GPIO 18 (input)
  configure TLS client with root CA
  connect to Wi-Fi (retry every 10 s until connected)
  nextSend = 0, retryDelay = 2 s

LOOP
  maintainWi-Fi:
      if not connected and 10 s elapsed -> reconnect

  if (now - lastSample) < 2000 ms -> continue

  samples = []
  repeat 5 times:
      trigger HC-SR04 (10 µs HIGH)
      width = pulseIn(ECHO, HIGH, 60 ms)
      if width > 0:
          d = width * 0.0343 / 2
          if 2 cm <= d <= empty + 8 cm: samples.append(d)
      wait 45 ms

  if len(samples) < 3:
      reading.valid = false
  else:
      median = median(samples)
      smoothed = (smoothed is unset) ? median : 0.35*median + 0.65*smoothed
      reading.distance = smoothed
      reading.height   = clamp(empty - smoothed, 0, tankHeight)
      reading.percent  = clamp(reading.height / tankHeight * 100, 0, 100)
      reading.valid    = true

  log reading to Serial Monitor

  if Wi-Fi connected and now >= nextSend:
      response = POST(apiUrl, reading, header x-api-key)
      if 200/201/202:
          retryDelay = 2 s
          nextSend = now
      else:
          nextSend = now + retryDelay
          retryDelay = min(retryDelay * 2, 30 s)
END LOOP
```

Server side:

```
ON POST /api/water-level:
    if key invalid -> 401
    parse and validate JSON (types, ranges, deviceId)
    if invalid -> 400
    mark device online, update lastSeen
    if reading.valid = false:
        emit sensor:error, emit device:status -> 202
    else:
        recompute height and percent
        store in memory, increment counter
        emit water:update, emit device:status -> 200
EVERY 2 s: if online and (now - lastSeen) > 15 s -> mark offline, emit device:status
ON socket connection: emit meta, device:status, latest water:update
```

---

## 15. Flowchart

```
        ┌─────────────┐
        │   START     │
        └──────┬──────┘
               ▼
     ┌───────────────────┐
     │ Init ports, TLS,  │
     │ connect Wi-Fi     │
     └─────────┬─────────┘
               ▼
      ┌─────────────────┐         ┌──────────────────┐
      │ 2 s elapsed?    │──no────▶│ loop (check Wi-Fi│
      └────────┬────────┘         │  reconnect)      │
               │ yes              └────────┬─────────┘
               ▼                           │
     ┌───────────────────┐                 │
     │ Take 5 pings,     │◀────────────────┘
     │ discard outliers  │
     └─────────┬─────────┘
               ▼
     ┌───────────────────┐
     │ ≥3 valid samples? │──no──▶ mark valid=false
     └────────┬──────────┘              │
              │ yes                     │
              ▼                         │
     ┌───────────────────┐              │
     │ median + EMA      │              │
     │ height and %      │              │
     └─────────┬─────────┘              │
               ▼                        ▼
     ┌────────────────────────────────────┐
     │ Wi-Fi connected? ──no──▶ skip POST │
     └─────────┬──────────────────────────┘
               │ yes
               ▼
     ┌───────────────────┐   401/400/5xx
     │ HTTPS POST JSON   │──────────────▶ backoff, retry later
     └─────────┬─────────┘
               │ 200
               ▼
     ┌───────────────────┐
     │ Backend validates │
     │ stores + emits    │
     └─────────┬─────────┘
               ▼
     ┌───────────────────┐
     │ Browser updates   │
     │ gauge + chart     │
     └─────────┬─────────┘
               ▼
            (LOOP)
```

---

## 16. Source Code Explanation

| File | Responsibility |
| --- | --- |
| `firmware/water_monitor/water_monitor.ino` | Sensor driver, filtering, unit conversion, JSON payload, HTTPS POST, Wi-Fi and HTTP reconnection |
| `backend/server.js` | Express app, authentication, validation, level recomputation, in-memory store, offline watchdog, Socket.IO, static hosting |
| `backend/public/index.html` | Dashboard structure and accessible card layout |
| `backend/public/css/style.css` | Theme tokens, gauge animation, responsive grid |
| `backend/public/js/dashboard.js` | Socket.IO client, REST fallback, DOM binding, Chart.js history, theme toggle |
| `backend/tools/simulate.js` | Fake ESP32 for testing without hardware |
| `render.yaml` | Infrastructure-as-code for the Render deployment |

### 16.1 Firmware

- **Configuration block** — Wi-Fi, backend host/port/path, API key, device id,
  sampling interval and tank calibration constants, each commented `CHANGE ME`.
- `wifiBegin()` / `wifiMaintain()` — `WiFi.persistent(false)` avoids flash wear,
  `setAutoReconnect(true)` plus a 10-second check in `loop()`.
- `pingCm()` — 10 µs trigger, `pulseIn` with a 60 ms timeout so a dead sensor can
  never block the loop; range-checks the result.
- `readFilteredCm()` — collects 5 samples, insertion-sorts them, takes the median,
  applies the EMA, and reports how many samples were valid.
- `waterHeightCm()`, `percentFor()` — calibration maths with clamping; the
  backend repeats the same formula from `distanceCm`, `tankHeightCm` and
  `emptyDistanceCm`.
- `buildPayload()` — fixed 320-byte buffer, `snprintf` formatting, field names
  identical to the backend's validator.
- `postReading()` — `HTTPClient` with certificates, 8 s timeout, `x-api-key`,
  returns true only for 2xx.
- `loop()` — no `delay()` longer than a few ms except the sensor settle gaps;
  all scheduling is `millis()`-based; failed uploads back off 2 → 4 → 8 → 16 →
  30 s and reset on success.

### 16.2 Backend

- `validateReading()` — checks the JSON object, device id pattern, numeric ranges
  and required fields; returns a normalised object or a precise error.
- `safeEqual()` — constant-time key comparison with `crypto.timingSafeEqual`.
- Level recomputation — the server derives `waterHeightCm` and `percent` from the
  measured distance and the tank dimensions, so the published value is always
  physically possible.
- `state` — latest reading, counters, last-seen timestamp; nothing written to disk.
- Offline watchdog — an interval checks `lastSeenMs` against `OFFLINE_AFTER_MS` and
  emits a status change once.
- `emitSnapshot()` — sends `meta`, `device:status` and the latest reading to each
  new socket, so a page opened late is immediately correct.
- Graceful shutdown — `SIGTERM`/`SIGINT` close Socket.IO then the HTTP server, which
  Render expects during redeploys.
- `lib/alerts.js` — a small state machine (`ok` → `low` / `full` → `ok`) driven by
  the level of each accepted reading. Entering LOW requires < 25 %, leaving it
  requires ≥ 27 %; FULL behaves symmetrically at 90 % / 88 %. A 10-minute cooldown
  limits repeated messages. Delivery uses the free Telegram Bot API over HTTPS
  with an 8-second timeout. The call is fire-and-forget: a Telegram outage is
  logged and never changes the 200 response to the ESP32.

### 16.3 Frontend

- `paintReading()` updates the ring (`stroke-dashoffset`), the animated water
  height, the status chip, the five-segment scale, every sensor tile and the
  device rows in one pass.
- `LEVELS` implements the five thresholds (<5 empty, 5–25 low, 25–60 medium,
  60–90 high, ≥90 full).
- History is an array capped at 120 points (4 minutes at 2 s) — browser memory
  only, as required.
- `initChart()` builds a filled line chart; `applyChartTheme()` re-reads CSS
  variables when the theme switches.
- Transport: Socket.IO with WebSocket then polling; on `disconnect` the page
  switches to 5-second REST polling, and returns to sockets automatically.
- `setInterval(…, 1000)` refreshes the relative "last updated" text and marks the
  device offline in the UI if data goes stale, independent of the server.

---

## 17. API Documentation

Base URL: `https://<service>.onrender.com`

| Method | Path | Auth | Purpose |
| --- | --- | --- | --- |
| POST | `/api/water-level` | `x-api-key` (device) | Ingest a reading |
| GET | `/api/water-level` | none | Latest reading + device summary |
| GET | `/api/status` | none | Server, device and alert notifier health |
| GET | `/api/health` | none | Liveness probe for Render |
| GET | `/` | none | Dashboard page |
| WS | `/socket.io` | none | Live events |

**POST request body**

```json
{
  "deviceId": "esp32-water-01",
  "valid": true,
  "distanceCm": 7.6,
  "tankHeightCm": 19.0,
  "emptyDistanceCm": 19.0,
  "samples": 5,
  "rssi": -58,
  "uptimeSec": 120,
  "firmware": "1.0.0"
}
```

**Success (200)**

```json
{ "ok": true, "accepted": true, "receivedAt": "2026-11-01T09:15:02.113Z", "percent": 60.0 }
```

**Errors**

| Code | Meaning | Example body |
| --- | --- | --- |
| 400 | Validation failed | `{"ok":false,"error":"tankHeightCm is required"}` |
| 401 | Bad/missing key | `{"ok":false,"error":"Invalid or missing device API key"}` |
| 404 | Unknown `/api/*` | `{"ok":false,"error":"Unknown API endpoint"}` |
| 500 | Unhandled error | `{"ok":false,"error":"Internal server error"}` |

**GET `/api/water-level`** returns `{ ok, data, device }` where `data` is the last
accepted reading and `device` contains `online`, `lastSeenAt`, `readings` and
`offlineAfterMs`; `data` is `null` before the first reading.

**Socket.IO events:** `meta`, `water:update`, `device:status`, `sensor:error`
(payloads in `README.md` section 7).

---

## 18. Dashboard Explanation

| Area | Contents |
| --- | --- |
| Header | Brand, Socket.IO pill (Live / Reconnecting), ESP32 pill (online/offline), last-updated clock, theme switch |
| Main card | Circular ring gauge + animated water fill + percentage + caption with water height and tank height |
| Tank status | Coloured chip (EMPTY/LOW/MEDIUM/HIGH/FULL), five-segment scale with ranges, contextual note, sensor-error banner |
| Sensor information | Water height, raw distance, tank height, empty distance, echo samples, capacity used |
| History graph | Chart.js line/area chart, last 4 minutes (120 points × 2 s), tooltip in %, updates in place |
| Device information | ESP32 state, Wi-Fi RSSI with quality label, last sensor update, server connection, uptime, firmware |
| Footer | Data-flow reminder and "no database" note |

**Design decisions**

- Dark theme by default with a light alternative stored in `localStorage`.
- Water-blue palette; each level has its own semantic colour (red → amber → blue
  → teal → green) so the state is readable at a glance.
- CSS grid (12 columns) collapses to 6 and then 1 column; the gauge scales with
  `min()` and `aspect-ratio`.
- Animations respect `prefers-reduced-motion`.
- No decorative elements beyond the water animation; every card carries data.

**Expected view (description for the report)**

> On a laptop the page shows a single row of three cards: the circular gauge with
> a large blue water fill reading "60%", the status card with the MEDIUM chip
> highlighted, and the sensor card with six tiles. Below, a wide chart card shows
> a smooth blue area graph rising gently; to its right the device card lists
> "Online", "-58 dBm · Excellent", "Connected (Socket.IO)". On a phone the cards
> stack vertically, the header wraps into two rows and the gauge stays circular at
> 64 % of the viewport width.

---

## 19. Testing and Results

### 19.1 What was verified in this repository

| Test | Method | Result |
| --- | --- | --- |
| Server starts on `PORT` | `npm run dev` | Pass — logs the port and configuration |
| Health endpoint | `GET /api/health` | Pass — `{"ok":true,"status":"healthy"}` |
| Ingestion without key | `POST` without header | Pass — 401 |
| Ingestion with key | simulator | Pass — 200, reading stored and broadcast |
| Invalid payload | `distanceCm: 9999` | Pass — 400 with field message |
| Level recomputation | distance 13.2 of 19 cm | 5.8 cm / 30.5 % (matches the formula) |
| Sensor-error path | `"valid": false` | Pass — 202, no level change |
| Offline detection | stop the simulator | Pass — device flips offline after 15 s |
| Alert thresholds | mock Telegram API + POSTs at 20 %, 19 %, 26 %, 30 %, 95 %, 85 % | Pass — LOW sent once, hysteresis suppressed 19 %/26 %, FULL sent once |
| Alert cooldown | same sequence with 10-minute cooldown | Pass — only the first message sent |
| Alerts disabled | no bot token | Pass — server runs, ingestion 200, notifier state `enabled:false` |
| Live dashboard update | Socket.IO broadcast observed | Pass — gauge and chart update without reload |
| Static hosting | `GET /` | Pass — dashboard served from `backend/public` |

### 19.2 Procedures that require physical hardware / deployment

These must be executed by the student; they were **not** performed here and
should not be reported as passed until observed:

- HC-SR04 distance accuracy against a ruler at 5, 10, 15, 20 cm.
- Serial Monitor output on the real board (Wi-Fi, `[Sensor]`, `[HTTP] 200`).
- ESP32 → Render HTTPS connection and certificate validation.
- Router power-cycle reconnection test.
- Access to the public URL from a phone on mobile data.
- 24-hour stability / memory soak test.

Suggested result table to fill in during the demo:

| Tank fill (measured) | Distance (cm) | Height (cm) | Reported % | Error |
| --- | --- | --- | --- | --- |
| Empty | | | | |
| 25 % | | | | |
| 50 % | | | | |
| 75 % | | | | |
| Full | | | | |

### 19.3 Observations expected

- Distance error within ±0.5 cm for still water between 5 and 30 cm.
- Occasional single-sample outliers suppressed by the median; output deviates
  by less than 1 % on a calm surface.
- Ripples cause ±0.3 cm noise, absorbed by the EMA.
- End-to-end latency one sampling period on a stable network; up to one extra
  sampling period on a cold Render instance.

---

## 20. Advantages

1. **Remote visibility** — the level is available anywhere over the internet, not
   just on the local Wi-Fi.
2. **No proprietary app or account** — a browser is enough; no Blynk subscription.
3. **Database-free** — no storage cost, no schema, no data-loss risk, simple
   deployment.
4. **Low cost** — ESP32 and HC-SR04 cost under ₹700; hosting is free.
5. **Non-contact sensing** — no corrosion, no contamination, no moving parts.
6. **Real-time** — WebSocket push, no polling or page refresh needed.
7. **Robust** — API key authentication, server-side validation and clamping,
   median + EMA filtering, Wi-Fi and HTTP reconnection, offline detection.
8. **Responsive** — one dashboard for phone, tablet and desktop, dark or light.
9. **Extensible** — REST API allows adding alerts, a second tank or a mobile app.
10. **Reproducible** — pinned dependencies, blueprint file, step-by-step docs.

---

## 21. Limitations

1. The HC-SR04 needs a minimum 3 cm gap; levels closer than that are read as
   invalid, so 100 % is approximated rather than exact.
2. No persistent history — restarting the server clears the latest reading, and
   the graph is limited to browser memory (4 minutes), as specified.
3. Public `GET` endpoints are unauthenticated: anyone with the URL can read the
   level (they cannot write).
4. Render's free tier sleeps after inactivity and has a slow cold start.
5. Ultrasonic readings can be disturbed by strong air currents, foam, steam or
   very narrow tanks.
6. A single device per server by default; multi-tank support would need an
   extended data model.
7. No pump control or automatic cut-off — monitoring only.
8. Wi-Fi 2.4 GHz only; no cellular fallback.

---

## 22. Future Scope

1. **Pump automation** — add a relay to start/stop the motor at LOW/FULL
   thresholds, with hysteresis.
2. **Richer alerting** — email/SMS in addition to Telegram, per-user subscriptions,
   quiet hours and delivery history.
3. **Persistence** — optional time-series storage (SQLite or a hosted Postgres) to
   keep weeks of history and consumption analytics.
4. **Multi-tank dashboard** — device registry and selectable tanks.
5. **Machine learning (MCA AIML focus)** — forecast the next filling time from the
   consumption curve; anomaly detection to distinguish a leak (steady unexplained
   drop) from normal usage; clustering of daily usage patterns.
6. **Solar + battery power** and deep-sleep duty cycling for an off-grid install.
7. **Better sensing** — JSN-SR04T waterproof probe for outdoor tanks; ToF sensor
   for a smaller blind zone.
8. **PWA / mobile app** with push notifications and offline caching.
9. **Access control** — read authentication via a shared dashboard password.
10. **Calibration wizard** in the dashboard to write tank dimensions to the device.

---

## 23. Conclusion

The project demonstrates a complete, production-conscious IoT pipeline built with
low-cost hardware and free cloud services. An ESP32 turns raw ultrasonic echoes
into a filtered water level, sends it over authenticated HTTPS every two seconds,
and a Node.js service validates and broadcasts it to a responsive dashboard where
the level, trend and device health are visible from anywhere. The design meets all
stated requirements: configurable tank dimensions, robust sensor handling,
multiple-sample filtering, automatic Wi-Fi and server reconnection, real-time
Socket.IO updates, API-key security, HTTPS in production, and no database. The
system is reproducible from the repository, is documented for deployment and
viva, and provides a clear foundation for the next steps of pump automation,
alerting and machine-learning-based consumption forecasting.

---

## 24. References

1. Espressif Systems, *ESP32 Series Datasheet*, v4.x — https://www.espressif.com
2. Espressif Systems, *Arduino-ESP32 core documentation* — https://docs.espressif.com/projects/arduino-esp32/
3. ElecFreaks, *HC-SR04 Ultrasonic Ranging Module Datasheet*.
4. Node.js documentation — https://nodejs.org/docs
5. Express.js guide — https://expressjs.com
6. Socket.IO documentation — https://socket.io/docs/v4/
7. Chart.js documentation — https://www.chartjs.org/docs/
8. Render documentation: *Web Services*, *Blueprint spec*, *TLS* — https://render.com/docs
9. MDN Web Docs: *Fetch API*, *WebSockets*, *CSS Grid* — https://developer.mozilla.org
10. Let's Encrypt, *Chain of Trust* — https://letsencrypt.org/certificates/
11. OWASP Foundation, *API Security Top 10* — https://owasp.org/API-Security/
12. Texas Instruments, *Ultrasonic Sensing Basics* application report.
