# Viva Questions and Answers

**Project:** Smart IoT Water Tank Monitoring System with Live Web Dashboard

Answers are deliberately short — enough for the examiner, with the deeper detail
in `PROJECT_REPORT.md`.

---

## A. Hardware and sensing

**1. Why did you choose the HC-SR04 instead of a float switch or a pressure sensor?**
The HC-SR04 is non-contact, so it never touches the water (no corrosion or
contamination), it gives a continuous value rather than only on/off, and it costs
about ₹120. A float switch gives only discrete levels; a pressure sensor needs
contact with water and careful sealing.

**2. What are the HC-SR04 specifications?**
5 V supply, ~15 mA, measuring range 2–400 cm, resolution about 3 mm, accuracy
about ±3 mm, 40 kHz burst, blind zone roughly 0–3 cm at the front of the sensor.

**3. Why is a voltage divider needed on the ECHO pin?**
The sensor is powered from 5 V and drives ECHO up to 5 V. ESP32 GPIOs are 3.3 V
and are not 5 V tolerant, so 1 kΩ + 2 kΩ divides 5 V to 3.33 V.

**4. What happens if the divider is omitted?**
The GPIO sees 5 V, which can damage the input over time or cause unreliable
readings; it may work briefly and then fail.

**5. Why is VCC connected to VIN and not 3V3?**
The HC-SR04 needs 5 V for a stable transmitter burst. At 3.3 V the range drops
and echoes are frequently missed.

**6. What is the sensor blind zone and how do you handle it?**
The sensor cannot measure closer than about 3 cm due to its own burst ringing.
The firmware discards readings below 2 cm and the tank is calibrated so 100 %
occurs at 2–3 cm, keeping the whole usable range outside the blind zone.

**7. Where exactly is the sensor mounted?**
Horizontally at the centre of the tank lid, facing the water, at least 3 cm above
the highest water level and 5 cm away from the walls so the burst is not
reflected from the sides.

**8. Why not use DHT or flow sensors?**
A flow sensor measures litres consumed, not the stock in the tank. The
requirement is the level inside the tank, which ultrasonic distance gives directly.

---

## B. Firmware

**9. Explain the distance formula.**
`distance = (echo pulse width in µs × speed of sound) / 2`. Speed of sound is
0.0343 cm/µs, so `cm = µs × 0.0343 / 2`. The division by two is because the sound
travels to the water and back.

**10. How do you calculate the water level and percentage?**
`height = emptyDistance − measuredDistance`, clamped to 0…tankHeight;
`percent = height / tankHeight × 100`, clamped to 0…100. The backend repeats the
same formula, so an arithmetic bug in the firmware cannot publish an impossible
value.

**11. Why take five readings and use the median instead of the average?**
Ultrasonic sensors occasionally produce a single wild echo (reflection or ripple).
The average is dragged towards that outlier; the median of five rejects it.
Then a light EMA smooths normal ripple noise without adding much lag.

**12. What is the EMA formula and what does alpha mean?**
`sₖ = α·mₖ + (1 − α)·sₖ₋₁` with α = 0.35. Higher α = faster response, more noise;
lower α = smoother, slower. Alpha 0.35 gives a ~3.7 s time constant at 2 s sampling.

**13. How do you handle an invalid reading?**
If fewer than three of five pings are valid, the payload is sent with
`"valid": false`; the backend replies 202, keeps the last good level, broadcasts a
sensor error and the dashboard shows a warning banner.

**14. Why is the code called non-blocking?**
There is no `delay(2000)`; the sketch keeps `lastReadAt` and `nextSendAt` with
`millis()` and returns from `loop()` immediately. Only the 45 ms inter-sample gaps
and the HTTP call block, which is unavoidable with the synchronous `HTTPClient`;
a production version could use an asynchronous client.

**15. How does the ESP32 reconnect to Wi-Fi?**
`WiFi.setAutoReconnect(true)` plus a 10-second check in `loop()` that calls
`WiFi.reconnect()`. `WiFi.persistent(false)` prevents flash wear from repeated
credential writes.

**16. What happens if Render is down or slow?**
The upload fails, the sketch waits 2 s, then doubles the delay up to 30 s
(exponential backoff) and resets on success. Sensor readings and Serial output
continue locally in the meantime.

**17. Why is the JSON payload built with `snprintf` instead of the ArduinoJson
library?**
The payload is fixed and small; `snprintf` into a 320-byte buffer avoids an extra
dependency and heap fragmentation. Field names are kept identical to the
server's schema.

**18. Why does the firmware use HTTPS and a root certificate?**
Without TLS anyone on the network could read the API key and inject fake levels.
`WiFiClientSecure` with `setCACert()` validates the server certificate, which
prevents man-in-the-middle attacks. `setInsecure()` exists only as a classroom
fallback.

---

## C. Backend

**19. Why Node.js + Express?**
Both the ESP32 upload endpoint and the browser dashboard speak HTTP; Express
serves both the REST API and the static page, and Node's event loop fits
Socket.IO's many long-lived connections with very low overhead.

**20. Why Socket.IO instead of plain WebSocket or polling?**
Socket.IO adds automatic reconnection, transport fallback (WebSocket → HTTP
long-polling), heartbeats and room support on top of WebSocket. Plain polling
would need a request every second and would add latency.

**21. Why is there no database?**
The requirement is the current level only, and the project must be deployable for
free. The latest reading in memory is enough; history lives in the browser tab.
The report lists optional persistence as future scope.

**22. How is the device authenticated?**
A shared secret is stored only in the server environment (`DEVICE_API_KEY`) and in
the firmware. The ESP32 sends it in `x-api-key`; the server compares it with
`crypto.timingSafeEqual` (constant time, prevents timing attacks). Wrong key → 401.

**23. Why is the API key not in the frontend JavaScript?**
The browser only needs read access. Anyone can view frontend source, so a key
there would be public and would let strangers upload fake readings. Ingestion
authentication stays server-side and device-side.

**24. Why does the server recalculate the percentage?**
Defence in depth: even with a firmware bug or a malicious client, the published
level is derived from the raw distance and clamped to 0…100, so the dashboard can
never display an impossible value.

**25. How does the system know the ESP32 went offline?**
Every accepted POST updates `lastSeenMs`. A watchdog interval compares it with
`OFFLINE_AFTER_MS` (15 s ≈ 6 missed uploads) and emits a `device:status` event
once when it flips.

**26. What validation does the backend perform?**
JSON body shape, `deviceId` pattern and expected id, numeric type checks,
`distanceCm` 0–500, `tankHeightCm`/`emptyDistanceCm` 0–500, `valid` flag handling,
optional field trimming, and a `Content-Type: application/json` body limit of 8 kB.

**27. What does a new browser tab receive on connect?**
The server emits `meta` (version, offline threshold, server time),
`device:status`, and the latest `water:update` if one exists — so a late joiner is
instantly in sync without waiting for the next sample.

**28. How does the server use `PORT`?**
`const PORT = Number(process.env.PORT) || 3000`. Render assigns the port at
runtime, so it must never be hard-coded; locally the default 3000 is used.

**29. What are the API endpoints?**
`POST /api/water-level` (device ingest, authenticated), `GET /api/water-level`
(latest), `GET /api/status` (server + device + alert health), `GET /api/health`
(liveness probe), `/` (dashboard), plus Socket.IO events.

**29a. How do the LOW/FULL alerts work?**
After each accepted reading a small state machine compares the level with
`ALERT_LOW_PERCENT` (25) and `ALERT_FULL_PERCENT` (90). A message is sent only when
the tank *enters* LOW or FULL, never for every reading. Two guards prevent spam:
hysteresis of 2 % (leaving LOW needs 27 %, leaving FULL needs 88 %) and a
10-minute cooldown. Delivery uses the free Telegram Bot API; the call is
fire-and-forget, so a Telegram failure is logged but the ESP32 still receives
HTTP 200. Without a bot token the whole feature is disabled and the rest of the
system is unchanged.

---

## D. Frontend

**30. How does the dashboard update without refreshing?**
The page subscribes to `water:update`; each event updates the ring dash offset,
water height CSS variable, status chip, tiles and appends a Chart.js point.

**31. What happens if the WebSocket drops?**
The `disconnect` handler shows "Reconnecting…", starts a 5-second REST poll of
`/api/water-level`, and Socket.IO reconnects automatically; on `connect` the poll
stops and live pushes resume.

**32. How is the water level graph limited?**
An array of at most 120 readings (2 s interval → 4 minutes). When full, the oldest
point is shifted out. Nothing is stored beyond the tab, as required.

**33. How does the design stay responsive?**
A 12-column CSS grid where cards span 4/8 columns on desktop, 6/12 on tablet and
12 on phones; the gauge uses `min(230px, 72vw)` with `aspect-ratio`; the chart
container has a fixed height and `maintainAspectRatio: false`.

**34. How are the five status levels defined?**
EMPTY < 5 %, LOW 5–25 %, MEDIUM 25–60 %, HIGH 60–90 %, FULL ≥ 90 %, each with its
own colour used consistently in the chip, the scale and the gauge ring.

**35. Why does the page still show "last updated 12s ago" if data stopped?**
A 1-second timer re-renders the relative timestamp and marks the device offline
when the newest reading is older than `offlineAfterMs`, so a silent failure is
visible even if the server's status event is delayed.

---

## E. Deployment and security

**36. How is the project deployed?**
The repository is pushed to GitHub and deployed on Render (Blueprint with
`render.yaml`, or a manual Node web service with root directory `backend`,
build `npm install`, start `npm start`). Render injects `PORT`, terminates TLS and
serves the service at `https://<name>.onrender.com`.

**37. What is `render.yaml`?**
An infrastructure-as-code blueprint: it declares the service type, runtime,
root directory, build/start commands, health check path and environment
variables (including a generated `DEVICE_API_KEY`) so deployment is repeatable.

**38. Does the free Render plan have limitations?**
Yes — the instance sleeps after about 15 minutes without traffic and cold starts
take 30–60 s; it has ephemeral disk and limited hours. Continuous 2-second uploads
keep it awake, and the firmware's retry logic handles a cold start.

**39. How is HTTPS ensured end to end?**
Render provides and auto-renews the TLS certificate; browsers use HTTPS; the ESP32
connects to port 443 and validates the certificate chain against the pinned root
CA.

**40. What would you change for a real building installation?**
Read authentication (shared login), a registered device list, persistent history
for analytics, a relay for pump control, alerting, and a UPS or solar supply.

---

## F. Project-level and AIML questions

**41. What is the novelty of your project compared to a Blynk demo?**
It is a self-owned full stack — the API, the real-time layer, the dashboard and the
deployment are all implemented and explained, with server-side validation,
authentication and offline detection. It does not depend on a proprietary app or
subscription, and the dashboard is public.

**42. How can machine learning be added?**
Three concrete directions: (a) time-series forecasting (ARIMA/Prophet/LSTM) of the
next fill event from the level curve; (b) anomaly detection — a steady unexplained
drop indicates a leak, while normal use has scheduled peaks; (c) clustering of
daily consumption profiles to classify households and recommend tank size. The
current REST API already exposes the data needed to build these models.

**43. What are the limitations of your data for ML?**
No persistence: only the last 4 minutes exist in the browser, so an ML pipeline
would first need a collector or a time-series store — which is listed in future
scope.

**44. How would you scale to 100 tanks?**
Add a device registry, key per device, a `deviceId` room in Socket.IO, and a
dashboard selector. For history, move to a time-series database. The stateless
server can then scale horizontally behind Render's load balancer.

**45. What was the hardest part of the project?**
Two things: making the sensor reliable (power quality, echoes, filtering) and
keeping the three layers consistent — identical field names, identical level
formula, and identical status thresholds on device, server and browser.
