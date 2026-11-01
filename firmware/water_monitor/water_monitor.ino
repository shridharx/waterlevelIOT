/*
 * ============================================================================
 *  Smart IoT Water Tank Monitoring System - ESP32 Firmware
 * ============================================================================
 *  Reads an HC-SR04 ultrasonic sensor mounted on top of a water tank, turns
 *  the distance into a water level (cm + %), and pushes the reading to the
 *  cloud backend over HTTPS every 2 seconds. The backend broadcasts the value
 *  to the public web dashboard through Socket.IO.
 *
 *  Board            : ESP32 DevKit V1 (ESP32 Dev Module)
 *  Core             : "esp32 by Espressif" 3.x
 *  Libraries        : WiFi, WiFiClientSecure, HTTPClient (all bundled)
 *
 *  Wiring (HC-SR04 is a 5 V device, ESP32 GPIOs are 3.3 V):
 *      HC-SR04 VCC  -> ESP32 VIN (5 V)
 *      HC-SR04 GND  -> ESP32 GND
 *      HC-SR04 TRIG -> GPIO 5
 *      HC-SR04 ECHO -> GPIO 18  via 1k/2k divider (ECHO -1k- GPIO18 -2k- GND)
 *
 *  =========================================================================
 *  VALUES YOU MUST CHANGE BEFORE UPLOADING  (marked "CHANGE ME")
 *  =========================================================================
 *    1. WIFI_SSID / WIFI_PASSWORD
 *    2. BACKEND_HOST   -> "<your-service>.onrender.com" (no https://)
 *    3. DEVICE_API_KEY -> same value as DEVICE_API_KEY on the backend
 *    4. DEVICE_ID      -> any unique name, e.g. "esp32-water-01"
 *    5. TANK_HEIGHT_CM / EMPTY_DISTANCE_CM -> your real tank measurements
 *
 *  Do not commit this file with your real Wi-Fi password or API key to a
 *  public GitHub repository. See README.md -> "Security".
 * ============================================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ============================================================================
//  1. CONFIGURATION - Wi-Fi
// ============================================================================
#define WIFI_SSID       "Independent_pg(2nd Floor)"        // CHANGE ME
#define WIFI_PASSWORD   "@Floor@123"    // CHANGE ME

// ============================================================================
//  2. CONFIGURATION - Cloud backend
// ============================================================================
// Use 1 for the deployed Render service (https), 0 only for a local server
// on your own LAN (http://192.168.x.x:3000).
#define USE_HTTPS          1

#define BACKEND_HOST       "your-service.onrender.com"   // CHANGE ME
#define BACKEND_PORT       443
#define API_PATH           "/api/water-level"

#define DEVICE_API_KEY     "REPLACE_WITH_YOUR_DEVICE_API_KEY"   // CHANGE ME
#define DEVICE_ID          "esp32-water-01"

#define FIRMWARE_VERSION   "1.0.0"

#define READ_INTERVAL_MS   2000UL   // sample + upload every 2 s
#define HTTP_TIMEOUT_MS    8000     // Render free tier can be slow on wake-up
#define HTTP_RETRY_MIN_MS  2000UL   // first retry delay after a failed POST
#define HTTP_RETRY_MAX_MS  30000UL  // retry backoff ceiling

// ============================================================================
//  3. CONFIGURATION - Sensor and tank calibration
// ============================================================================
#define TRIG_PIN           5
#define ECHO_PIN           18

// TANK_HEIGHT_CM     : the water depth, in cm, that represents 100 %.
// EMPTY_DISTANCE_CM  : the distance the sensor reports when the tank is EMPTY
//                      (sensor face -> tank bottom). This is the 0 % reference.
// Optional: set TANK_HEIGHT_CM a little smaller than EMPTY_DISTANCE_CM so the
// top of the measurable range can reach 100 % despite the 2-3 cm blind zone.
#define TANK_HEIGHT_CM     19.0f
#define EMPTY_DISTANCE_CM  19.0f

#define MIN_VALID_CM        2.0f    // HC-SR04 blind zone
// Anything beyond the empty distance by more than this is a ghost echo.
#define MAX_VALID_CM       (EMPTY_DISTANCE_CM + 8.0f)

#define SAMPLES_PER_READ    5       // pings averaged per cycle
#define SAMPLE_GAP_MS       45      // settle time between pings
#define SMOOTHING_ALPHA     0.35f   // EMA weight for the newest reading

// ============================================================================
//  4. TLS - root certificate used to validate the HTTPS server
// ============================================================================
// ISRG Root X1 (Let's Encrypt). Render serves *.onrender.com certificates from
// Let's Encrypt, and the ESP32 validates the full chain against this root.
// If your certificate is issued by another CA (for example Google Trust
// Services, which Render also uses), replace this PEM with that CA's root.
// README.md -> "Certificate troubleshooting" explains how to get it.
// Set to 0 ONLY for short experiments: it disables validation entirely.
#define TLS_VERIFY 1

static const char ROOT_CA[] PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)CERT";

// ============================================================================
//  5. Globals
// ============================================================================
static WiFiClientSecure secureClient;
static WiFiClient plainClient;

static bool  wifiConfigured = false;
static unsigned long lastWifiCheck   = 0;
static unsigned long lastScanAt      = 0;
static unsigned long lastReadAt      = 0;
static unsigned long nextSendAt      = 0;
static unsigned long retryDelayMs    = HTTP_RETRY_MIN_MS;

static float smoothedCm = -1.0f;   // -1 = no reading yet
static bool  sensorOk   = false;

static unsigned long bootMs = 0;

// ============================================================================
//  6. Wi-Fi
// ============================================================================
// Turns the ESP32 disconnect reason into something readable in the Serial
// Monitor (the numeric codes alone are not obvious).
static const __FlashStringHelper *wifiReasonText(uint8_t reason) {
  switch (reason) {
    case 2:   return F("AUTH_EXPIRE - check the password");
    case 15:  return F("4WAY_HANDSHAKE_TIMEOUT - wrong password");
    case 200: return F("BEACON_TIMEOUT - signal lost");
    case 201: return F("NO_AP_FOUND - SSID not visible: 2.4 GHz only, hotspot off, or wrong name");
    case 202: return F("AUTH_FAIL - wrong password");
    case 203: return F("ASSOC_FAIL - router rejected the board");
    case 204: return F("HANDSHAKE_TIMEOUT - router unreachable");
    case 205: return F("CONNECTION_FAIL - router refused the connection");
    default:  return F("see ESP32 Wi-Fi reason codes");
  }
}

static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    uint8_t reason = info.wifi_sta_disconnected.reason;
    Serial.printf("[WiFi] disconnected (reason %u: %s)\n", reason,
                  String(wifiReasonText(reason)).c_str());
    lastScanAt = 0;  // print a fresh scan on the next retry
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.print("[WiFi] got IP: ");
    Serial.println(WiFi.localIP());
    lastScanAt = 0;
  }
}

// Lists nearby 2.4 GHz networks when the board cannot join, so the Serial
// Monitor answers "is my SSID even visible?". Runs at most every 30 s and only
// while disconnected; a scan takes 1-2 s.
static void wifiDiagnose() {
  if (millis() - lastScanAt < 30000UL) return;
  lastScanAt = millis();

  Serial.println(F("[WiFi] scanning for 2.4 GHz networks..."));
  int found = WiFi.scanNetworks(false, true);  // synchronous, include hidden
  if (found <= 0) {
    Serial.println(F("[WiFi] no networks found - is the router/hotspot switched on?"));
    WiFi.scanDelete();
    return;
  }

  bool targetVisible = false;
  int shown = 0;
  for (int i = 0; i < found; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    if (ssid == WIFI_SSID) targetVisible = true;
    if (shown < 8) {
      Serial.printf("  %-24s %4d dBm  ch%-2d %s\n", ssid.c_str(), WiFi.RSSI(i),
                    WiFi.channel(i),
                    WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured");
      shown++;
    }
  }
  WiFi.scanDelete();

  if (targetVisible) {
    Serial.printf("[WiFi] \"%s\" is visible - check WIFI_PASSWORD\n", WIFI_SSID);
  } else {
    Serial.printf("[WiFi] \"%s\" NOT found. ESP32 sees 2.4 GHz only\n", WIFI_SSID);
  }
}

static void wifiBegin() {
  WiFi.persistent(false);          // do not rewrite credentials to flash
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);            // low latency for HTTP + socket updates
  WiFi.setAutoReconnect(true);
  if (!wifiConfigured) {
    WiFi.onEvent(onWifiEvent);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiConfigured = true;
  } else {
    WiFi.reconnect();
  }
}

// Non-blocking: called from loop() every 10 s.
static void wifiMaintain() {
  if (millis() - lastWifiCheck < 10000UL) return;
  lastWifiCheck = millis();

  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("[WiFi] not connected - reconnecting");
  wifiBegin();
  wifiDiagnose();
}

// ============================================================================
//  7. Sensor
// ============================================================================
// One HC-SR04 ping. Returns distance in cm, or -1 on timeout / invalid echo.
static float pingCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // ~34 ms round trip for 5 m; 60 ms timeout avoids blocking on a dead sensor.
  unsigned long us = pulseIn(ECHO_PIN, HIGH, 60000UL);
  if (us == 0) return -1.0f;

  float cm = us * 0.0343f / 2.0f;
  if (cm < MIN_VALID_CM || cm > MAX_VALID_CM) return -1.0f;
  return cm;
}

static void sortFloats(float *v, int n) {
  for (int i = 1; i < n; i++) {
    float key = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > key) {
      v[j + 1] = v[j];
      j--;
    }
    v[j + 1] = key;
  }
}

// Takes SAMPLES_PER_READ pings, keeps the median of the valid ones and applies
// a light exponential smoothing so the graph does not jump on a single echo.
// Returns the filtered distance in cm, or -1 when the sensor is unreliable.
static float readFilteredCm(int *usedSamples) {
  float valid[SAMPLES_PER_READ];
  int n = 0;

  for (int i = 0; i < SAMPLES_PER_READ; i++) {
    float cm = pingCm();
    if (cm > 0) valid[n++] = cm;
    delay(SAMPLE_GAP_MS);
  }
  *usedSamples = n;
  if (n < 3) {                    // majority of pings failed -> treat as error
    sensorOk = false;
    return -1.0f;
  }

  sortFloats(valid, n);
  float median = (n % 2) ? valid[n / 2]
                         : (valid[n / 2 - 1] + valid[n / 2]) / 2.0f;

  if (smoothedCm < 0) smoothedCm = median;
  else smoothedCm = SMOOTHING_ALPHA * median + (1.0f - SMOOTHING_ALPHA) * smoothedCm;

  sensorOk = true;
  return smoothedCm;
}

// Water depth in cm for a given measured distance.
static float waterHeightCm(float distanceCm) {
  float depth = EMPTY_DISTANCE_CM - distanceCm;
  if (depth < 0) depth = 0;
  if (depth > TANK_HEIGHT_CM) depth = TANK_HEIGHT_CM;
  return depth;
}

static float percentFor(float distanceCm) {
  float p = (waterHeightCm(distanceCm) / TANK_HEIGHT_CM) * 100.0f;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return p;
}

// ============================================================================
//  8. Upload
// ============================================================================
static String apiUrl() {
  String url = USE_HTTPS ? "https://" : "http://";
  url += BACKEND_HOST;
  url += ':';
  url += String(BACKEND_PORT);
  url += API_PATH;
  return url;
}

// Builds the JSON body. Field names must match backend/server.js exactly.
static String buildPayload(bool valid, float distanceCm, float waterCm,
                           float percent, int samples) {
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"deviceId\":\"%s\",\"valid\":%s,\"distanceCm\":%.1f,"
           "\"waterHeightCm\":%.1f,\"percent\":%.1f,"
           "\"tankHeightCm\":%.1f,\"emptyDistanceCm\":%.1f,"
           "\"samples\":%d,\"rssi\":%d,\"uptimeSec\":%lu,"
           "\"firmware\":\"%s\"}",
           DEVICE_ID,
           valid ? "true" : "false",
           valid ? distanceCm : -1.0f,
           valid ? waterCm : 0.0f,
           valid ? percent : 0.0f,
           TANK_HEIGHT_CM,
           EMPTY_DISTANCE_CM,
           samples,
           WiFi.RSSI(),
           (unsigned long)((millis() - bootMs) / 1000UL),
           FIRMWARE_VERSION);
  return String(buf);
}

// Returns true when the backend accepted the reading (HTTP 200/201/202).
static bool postReading(const String &payload) {
  HTTPClient http;
  bool started = false;

  if (USE_HTTPS) {
    started = http.begin(secureClient, apiUrl());
  } else {
    started = http.begin(plainClient, apiUrl());
  }
  if (!started) {
    Serial.println("[HTTP] begin() failed");
    return false;
  }

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", DEVICE_API_KEY);

  int code = http.POST(payload);

  if (code > 0) {
    String body = http.getString();
    Serial.printf("[HTTP] %d %s\n", code, body.substring(0, 120).c_str());
  } else {
    Serial.printf("[HTTP] POST failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();

  return code == 200 || code == 201 || code == 202;
}

// ============================================================================
//  9. Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  bootMs = millis();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F(" Smart Water Tank Monitor - ESP32"));
  Serial.println(F("=========================================================="));
  // Catch the placeholders before they waste an upload + debugging session.
  if (strcmp(WIFI_SSID, "YOUR_WIFI_NAME") == 0) {
    Serial.println(F("[CONFIG] WIFI_SSID is still the placeholder"));
  }
  if (strcmp(DEVICE_API_KEY, "REPLACE_WITH_YOUR_DEVICE_API_KEY") == 0) {
    Serial.println(F("[CONFIG] DEVICE_API_KEY is still the placeholder"));
  }
  if (strcmp(BACKEND_HOST, "your-service.onrender.com") == 0) {
    Serial.println(F("[CONFIG] BACKEND_HOST is still the placeholder"));
  }

  Serial.print(F("[WiFi] MAC: "));
  Serial.println(WiFi.macAddress());
  Serial.printf("Device       : %s (fw %s)\n", DEVICE_ID, FIRMWARE_VERSION);
  Serial.printf("Backend      : %s\n", apiUrl().c_str());
  Serial.printf("Tank height  : %.1f cm (empty distance %.1f cm)\n",
                TANK_HEIGHT_CM, EMPTY_DISTANCE_CM);
  Serial.printf("Sample cycle : %lu ms\n", READ_INTERVAL_MS);

#if USE_HTTPS
#if TLS_VERIFY
  secureClient.setCACert(ROOT_CA);
  Serial.println(F("TLS          : enabled, certificate validation ON"));
#else
  secureClient.setInsecure();
  Serial.println(F("TLS          : enabled, CERTIFICATE VALIDATION DISABLED"));
#endif
#else
  Serial.println(F("TLS          : disabled (plain HTTP)"));
#endif

  Serial.print(F("[WiFi] connecting to "));
  Serial.println(WIFI_SSID);
  wifiBegin();
}

// ============================================================================
// 10. Loop - all timing with millis(), no long delays
// ============================================================================
void loop() {
  wifiMaintain();

  bool due = (millis() - lastReadAt >= READ_INTERVAL_MS);
  if (!due) {
    delay(5);
    return;
  }
  lastReadAt = millis();

  int samples = 0;
  float cm = readFilteredCm(&samples);

  bool valid = (cm > 0);
  float depth = valid ? waterHeightCm(cm) : 0.0f;
  float pct   = valid ? percentFor(cm) : 0.0f;

  // Local debugging output, always visible even with no network.
  if (valid) {
    Serial.printf("[Sensor] %.1f cm | water %.1f/%.1f cm | %.1f%% | samples %d/%d | RSSI %d\n",
                  cm, depth, TANK_HEIGHT_CM, pct, samples, SAMPLES_PER_READ, WiFi.RSSI());
  } else {
    Serial.printf("[Sensor] no valid echo (%d/%d samples) - sending error status\n",
                  samples, SAMPLES_PER_READ);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] skipped - Wi-Fi offline");
    return;
  }
  if (millis() < nextSendAt) return;

  bool ok = postReading(buildPayload(valid, cm, depth, pct, samples));

  if (ok) {
    retryDelayMs = HTTP_RETRY_MIN_MS;
    nextSendAt = millis();
  } else {
    nextSendAt = millis() + retryDelayMs;
    Serial.printf("[HTTP] retrying in %lu ms\n", retryDelayMs);
    retryDelayMs = min(retryDelayMs * 2UL, HTTP_RETRY_MAX_MS);
  }
}
