# Presentation Outline, Demo Script and Expected Output

---

## 1. Slide outline (12 slides, ~10 minutes)

### Slide 1 — Title
**Smart IoT Water Tank Monitoring System with Live Web Dashboard**
ESP32 · HC-SR04 · Node.js · Socket.IO · Render
Name, roll number, guide, department, year.

### Slide 2 — Problem
- Tank level is checked manually → overflow, dry running, wasted water.
- No remote visibility, no history.
- Blynk-style apps need a proprietary app and an account.
- Existing local ESP32 pages work only on the same Wi-Fi.

### Slide 3 — Objectives
Measure → convert → transmit → validate → display live → anywhere, for free, with
no database.

### Slide 4 — Hardware and wiring
Photo/diagram of ESP32 + HC-SR04, the 1 kΩ/2 kΩ divider, pin table
(VCC→VIN, GND→GND, TRIG→GPIO5, ECHO→GPIO18).
Call out: *ECHO must be divided to 3.3 V.*

### Slide 5 — System architecture
`Sensor → ESP32 (filter, compute) → HTTPS → Render (Express + Socket.IO) → browser`.
Mention the return path is WebSocket push, not polling.

### Slide 6 — Measurement maths
`d = t × 0.0343 / 2`, `height = empty − d`, `percent = height / tankHeight × 100`.
Filtering: 5 pings → discard outliers → median → EMA (α = 0.35).

### Slide 7 — Firmware
Key blocks: non-blocking loop, 2 s cycle, TLS with root CA, API-key header,
exponential backoff, Wi-Fi auto-reconnect. Show the Serial Monitor line.

### Slide 8 — Backend and API
Endpoint table, API-key auth, validation + server-side recalculation, in-memory
state, offline watchdog, Socket.IO events.

### Slide 9 — Dashboard
Screenshot or live page: circular gauge, status band, sensor tiles, live chart,
device health, dark/light, responsive.

### Slide 10 — Testing
Local API tests and hardware/calibration table with measured vs reported level.
State clearly what was tested and what was not.

### Slide 11 — Advantages, limitations, future scope
Cheap, open, database-free, real-time. Limitations: 3 cm blind zone, no history,
public read access, free-tier sleep. Future: pump relay, alerts, persistence,
multi-tank, ML forecasting and leak detection.

### Slide 12 — Conclusion and references
Working end-to-end prototype, documented and reproducible. Thank the panel.

**Backup slides:** full code of the sensor function, `render.yaml`, API request/
response, troubleshooting table.

---

## 2. Demonstration script (for faculty, ~6 minutes)

**Before the panel arrives**

1. Backend running (Render URL open in a tab, or `npm run dev` locally).
2. Dashboard open on the laptop, second tab open on a phone on mobile data.
3. ESP32 powered and connected; Serial Monitor at 115200 visible on a second
   screen or split window.
4. A transparent bottle/tank with water and a measuring scale nearby.
5. The simulator command ready in a terminal as a fallback (`npm run simulate`).

**Running order**

> 1. **"This is the problem we are solving."**
>    Point at the tank. *"Today nobody knows the level without climbing up. The
>    result is overflow or a dry-running pump."*
>
> 2. **"This is the hardware."**
>    Show the ESP32 and the sensor, point to the divider. *"ECHO is 5 volts, so the
>    1 kΩ and 2 kΩ resistors bring it to 3.3 volts — otherwise the ESP32 is
>    damaged."*
>
> 3. **"This is the live value."**
>    Show the Serial Monitor: `[Sensor] 12.4 cm | water 6.6/19.0 cm | 34.7% |
>    samples 5/5`. *"Five pings per cycle, the median is kept, then a small
>    exponential smooth."*
>
> 4. **"This is what the user sees."**
>    Show the dashboard. *"The percentage, the animated tank, the five-level
>    status, water height, raw distance, echo samples, Wi-Fi signal."*
>
> 5. **"Watch it update."** Pour water slowly for 10–15 seconds.
>    Gauge rises, chart draws new points, no refresh. *"The browser is not polling;
>    the server pushes each reading over a WebSocket within milliseconds of the
>    upload."*
>
> 6. **"From anywhere."** Pick up the phone on mobile data, open the Render URL.
>    *"Same value, no app, no login."* Use the theme button.
>
> 7. **"When something breaks."** Unplug the sensor (or run the `valid:false`
>    request). *"The firmware reports a sensor error, the server keeps the last
>    good value and warns the user instead of showing a false 0%."* Reconnect.
>
> 8. **"When the network breaks."** Stop the backend with `Ctrl+C` for 15 seconds.
>    *"The dashboard says Reconnecting, and the server marks the device offline."*
>    Restart it: *"Both recover automatically — no page reload, no ESP32 reset."*
>
> 9. **"Security."** Open DevTools → Network. *"There is no API key in the browser
>    code; the key exists only on the server and in the firmware. Uploads without
>    it get 401."*
>
> 10. **Close.** *"ESP32 to public dashboard in under two seconds, under ₹700 of
>     hardware, free hosting, no database."*

**Fallback if hardware fails:** run `npm run simulate` and say *"the simulator
reproduces the exact ESP32 payload so the cloud chain can still be demonstrated."*

---

## 3. Expected output / screenshot descriptions

### 3.1 Serial Monitor (115200)

```
==========================================================
 Smart Water Tank Monitor - ESP32
==========================================================
Device       : esp32-water-01 (fw 1.0.0)
Backend      : https://aquasense.onrender.com:443/api/water-level
Tank height  : 19.0 cm (empty distance 19.0 cm)
Sample cycle : 2000 ms
TLS          : enabled, certificate validation ON
[WiFi] connecting to HomeWiFi
[WiFi] got IP: 192.168.1.42
[Sensor] 7.6 cm | water 11.4/19.0 cm | 60.0% | samples 5/5 | RSSI -58
[HTTP] 200 {"ok":true,"accepted":true,"receivedAt":"2026-11-01T09:15:02.113Z","percent":60}
[Sensor] 7.5 cm | water 11.5/19.0 cm | 60.5% | samples 5/5 | RSSI -58
```

### 3.2 Dashboard — desktop (dark)

- Header: droplet logo, **AquaSense**, subtitle; right side pills **● Live** and
  **● ESP32 online**, "Last updated 1s ago", theme button.
- Level card: circular ring almost three-quarters full, water fill with two slow
  waves, big **60%**, caption "11.4 cm of water in the tank".
- Status card: **MEDIUM** chip in blue; the five-segment scale highlights MEDIUM.
- Sensor card: six tiles — Water height 11.4 cm, Raw distance 7.6 cm, Tank height
  19.0 cm, Empty distance 19.0 cm, Echo samples 5 pings, Capacity 60 %.
- Chart card: smooth blue area line rising across the last four minutes, y-axis
  0–100 %.
- Device card: Online (green), "-58 dBm · Excellent", last sensor update
  09:15:02, "Connected (Socket.IO)", uptime "4m 12s", firmware 1.0.0.

### 3.3 Dashboard — phone (light theme)

Single column: header wraps to two rows, gauge at ~64 % of viewport width, then
status, sensor tiles in two columns, chart, device card. No horizontal scrolling.

### 3.4 API responses

```
GET /api/health
{ "ok": true, "status": "healthy", "uptimeSec": 812, "now": "2026-11-01T09:15:02.113Z" }

GET /api/water-level
{ "ok": true,
  "data": { "deviceId": "esp32-water-01", "distanceCm": 7.6, "waterHeightCm": 11.4,
            "percent": 60, "tankHeightCm": 19, "emptyDistanceCm": 19, "samples": 5,
            "rssi": -58, "uptimeSec": 252, "firmware": "1.0.0",
            "receivedAt": "2026-11-01T09:15:02.113Z" },
  "device": { "online": true, "readings": 126, "offlineAfterMs": 15000 } }

POST /api/water-level without key
401 { "ok": false, "error": "Invalid or missing device API key" }
```

### 3.5 Demo checklist

- [ ] Serial Monitor shows Wi-Fi IP and `[HTTP] 200`
- [ ] Dashboard percentage changes when water is poured
- [ ] Phone on mobile data shows the same value
- [ ] Level chip changes across a threshold (e.g. MEDIUM → HIGH)
- [ ] Chart gains points without a refresh
- [ ] Stop backend → "Reconnecting…" and ESP32 offline badge
- [ ] Start backend → recovers automatically
- [ ] `POST` without key → 401 shown in Postman
