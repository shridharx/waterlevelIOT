#define BLYNK_TEMPLATE_ID "TMPL3n-Z3u2qE"
#define BLYNK_TEMPLATE_NAME "WATER LABEL"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <BlynkSimpleEsp32.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <time.h>

#include "Dashboard.h"

#define TRIG_PIN 5
#define ECHO_PIN 18
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ---------------------------------------------------------------
//  CALIBRATION - distance from sensor face, in cm
//  TANK = cut water bottle (17 cm tall), used as the final test tank.
//  EMPTY = sensor -> bottle bottom, FULL = sensor -> water when full.
// ---------------------------------------------------------------
#define EMPTY_DISTANCE_CM 19.6   // measured: sensor -> bottle bottom (0%)
#define FULL_DISTANCE_CM   3.0   // near the rim (100%), above the 2-3cm dead zone
#define OUT_OF_RANGE_CM   (EMPTY_DISTANCE_CM + 8.0)  // >27.6cm = ghost echo, ignore
#define MIN_VALID_CM       2.0

#define BLYNK_VPIN_LEVEL V0
#define HIGH_LEVEL_PERCENT 80

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------------------------------------------------------
//  Wi-Fi credentials
// ---------------------------------------------------------------
#define SSID_BASE   "YOUR_WIFI_NAME"
char ssid[33];
const char* pass = "YOUR_WIFI_PASSWORD";

BlynkTimer timer;
bool notified = false;
bool oledOk = false;
float filteredCm = -1.0f;   // low-pass filtered distance, -1 = unset

// Latest values, cached so the display can be redrawn on its own
// (needed for the blinking alarm) without re-pinging the sensor.
int   dispPercent = 0;
float dispCm      = 0;
int   dispSamples = 0;
bool  dispValid   = false;

const unsigned long WIFI_BOOT_TIMEOUT_MS    = 15000;
const unsigned long WIFI_RETRY_INTERVAL_MS  = 15000;
const unsigned long BLYNK_RETRY_INTERVAL_MS = 10000;

unsigned long lastWifiAttempt  = 0;
unsigned long lastBlynkAttempt = 0;

// ---------------- WEB DASHBOARD / HISTORY ----------------

#define HISTORY_CAPACITY 10080                        // 7 days of 1-minute averages
#define HISTORY_SAVE_INTERVAL_MS (10UL * 60UL * 1000UL)
#define HISTORY_FILE "/history.bin"
#define HISTORY_MAGIC 0x574C4831UL                    // "WLH1"

WebServer webServer(80);

uint32_t histMinute[HISTORY_CAPACITY];                // epoch minutes
uint8_t  histPct[HISTORY_CAPACITY];                   // average percent
uint8_t  histCm[HISTORY_CAPACITY];                    // average distance (cm)

uint16_t histHead  = 0;                               // next write slot
uint16_t histCount = 0;                               // valid points

bool histDirty     = false;
bool histSynthetic = false;
bool fsReady       = false;
unsigned long lastHistorySave = 0;

// Current-minute aggregation
uint32_t aggMinute  = 0;
uint32_t aggSumPct  = 0;
uint32_t aggSumCm10 = 0;
uint16_t aggCount   = 0;
bool     aggActive  = false;

bool clockSynced = false;
bool webStarted  = false;

struct HistoryHeader {
  uint32_t magic;
  uint16_t count;
  uint16_t head;
  uint8_t  synthetic;
  uint8_t  reserved[3];
};

bool scanForTarget(char* outSsid, uint8_t* outBssid, int32_t* outChannel, bool* outOpen);

// Web dashboard / history
void setupWebDashboard();
void handleRoot();
void handleLive();
void handleHistory();
void handleExportCsv();
void handleClearHistory();
void historyRecord(int percent, float cm);
void historyFlushMinute();
void historyAppend(uint32_t minute, uint8_t pct, uint8_t cm);
void historyClear();
void historyLoad();
void historySave();
void historyMaintain();
bool clockIsValid();
uint32_t minuteNow();
void syncClock();

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    uint8_t r = info.wifi_sta_disconnected.reason;
    if (r != 36) {  // 36 = our own disconnect, not interesting
      Serial.print(F("[WiFi] disconnect reason="));
      Serial.println(r);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  snprintf(ssid, sizeof(ssid), "%s", SSID_BASE);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) {
    Serial.println(F("OLED init failed - continuing without display"));
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Water Monitor Init");
    display.display();
  }

  Blynk.config(BLYNK_AUTH_TOKEN);

  // Credentials are compiled in, so keep them out of NVS. Without this the
  // core defaults to WIFI_STORAGE_FLASH and every retry (every 15 s while the
  // hotspot is asleep) rewrites the Wi-Fi config to flash.
  WiFi.persistent(false);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.onEvent(onWifiEvent);
  Serial.print(F("[WiFi] Connecting to \""));
  Serial.print(ssid);
  Serial.print(F("\" (len "));
  Serial.print(strlen(ssid));
  Serial.println(F(")"));

  uint8_t b[6];
  int32_t  ch = 0;
  char     found[33];
  bool     isOpen = false;
  if (scanForTarget(found, b, &ch, &isOpen)) {
    Serial.print(F("[WiFi] AP visible: \""));
    Serial.print(found);
    Serial.print(F("\" ch "));
    Serial.print(ch);
    Serial.print(F("  security: "));
    Serial.println(isOpen ? F("OPEN") : F("WPA2"));
    joinAp(found, b, ch, isOpen);
  } else {
    Serial.println(F("[WiFi] AP not visible yet"));
    WiFi.begin(ssid, pass);
  }

  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - started) < WIFI_BOOT_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[WiFi] Connected, IP: "));
    Serial.println(WiFi.localIP());
    Blynk.connect(3000);
    lastBlynkAttempt = millis();
  } else {
    Serial.println(F("[WiFi] Not connected yet - running offline, will retry"));
  }
  lastWifiAttempt = millis();

  // Local dashboard + history (works even when Blynk is offline)
  historyLoad();
  setupWebDashboard();
  syncClock();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[Web] Dashboard: http://"));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("[Web] Dashboard: connect Wi-Fi to view"));
  }

  timer.setInterval(1000L, measureWaterLevel);   // live level, once per second
  timer.setInterval(400L, alertRefresh);   // keeps the high-level alarm blinking
}

void loop() {
  maintainNetwork();
  if (Blynk.connected()) {
    Blynk.run();
  }
  timer.run();

  if (webStarted) {
    webServer.handleClient();
  }

  historyMaintain();
}

void maintainNetwork() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiAttempt >= WIFI_RETRY_INTERVAL_MS) {
      lastWifiAttempt = millis();
      Serial.println(F("[WiFi] Retrying..."));

      // A plain begin() fails with 201 if the AP is not awake at that instant.
      // Scan first, and if the SSID is there, lock onto its exact BSSID/channel.
      uint8_t b[6];
      int32_t  ch = 0;
      char     found[33];
      bool     isOpen = false;
      if (scanForTarget(found, b, &ch, &isOpen)) {
        Serial.print(F("[WiFi] AP found \""));
        Serial.print(found);
        Serial.print(F("\" ch "));
        Serial.print(ch);
        Serial.print(F("  security: "));
        Serial.println(isOpen ? F("OPEN") : F("WPA2"));
        joinAp(found, b, ch, isOpen);
      } else {
        Serial.println(F("[WiFi] AP not in scan (hotspot off?) - will retry"));
        WiFi.disconnect(true);
        delay(500);
        WiFi.begin(ssid, pass);
      }
    }
    return;
  }

  if (!Blynk.connected() && (millis() - lastBlynkAttempt >= BLYNK_RETRY_INTERVAL_MS)) {
    lastBlynkAttempt = millis();
    Serial.println(F("[Blynk] Connecting..."));
    Blynk.connect(3000);
  }
}

bool scanForTarget(char* outSsid, uint8_t* outBssid, int32_t* outChannel, bool* outOpen) {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    // Tolerate a stray trailing space: some APs broadcast it and it is
    // invisible in phone UIs, which would otherwise make the scan miss.
    String a = s;
    while (a.length() && a.charAt(a.length() - 1) == ' ') a.remove(a.length() - 1);
    if (strcmp(a.c_str(), SSID_BASE) == 0) {
      strncpy(outSsid, s.c_str(), 32);
      outSsid[32] = '\0';
      memcpy(outBssid, WiFi.BSSID(i), 6);
      *outChannel = WiFi.channel(i);
      *outOpen    = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      return true;
    }
  }
  return false;
}

// Joins using the security mode actually reported by the scan: an open
// hotspot must be joined with no passphrase, otherwise the AP answers
// with reason 210 (found, but security incompatible).
void joinAp(const char* apSsid, const uint8_t* b, int32_t ch, bool isOpen) {
  WiFi.disconnect(true);
  delay(350);
  if (isOpen) WiFi.begin(apSsid, NULL, ch, b, true);
  else        WiFi.begin(apSsid, pass, ch, b, true);
}

// ---------------------------------------------------------------
//  HC-SR04 needs >= 60 ms between pings; shorter gaps produce
//  0 cm timeouts and 10 cm phantom echoes from the previous burst.
// ---------------------------------------------------------------
long pingOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  return pulseIn(ECHO_PIN, HIGH, 30000);
}

// Weak-echo tolerant read: keep pinging until we collect valid samples that
// fall inside the tank window, then take the median. With a healthy 5V supply
// this resolves on the first sample; on a marginal setup it still gets a value.
bool readDistanceCm(float* outCm, int* outSamples, int* outTries) {
  float vals[5];
  int   n = 0;
  int   tries = 0;

  while (n < 5 && tries < 14) {
    tries++;
    long us = pingOnce();
    delay(60);

    if (us == 0) continue;
    float cm = us * 0.034f / 2.0f;
    if (cm < MIN_VALID_CM || cm > OUT_OF_RANGE_CM) continue;  // ghost / no target
    vals[n++] = cm;
  }

  *outSamples = n;
  *outTries   = tries;
  if (n == 0) return false;

  for (int i = 1; i < n; i++) {           // insertion sort, tiny array
    float k = vals[i];
    int j = i - 1;
    while (j >= 0 && vals[j] > k) { vals[j + 1] = vals[j]; j--; }
    vals[j + 1] = k;
  }
  *outCm = vals[n / 2];
  return true;
}

void measureWaterLevel() {
  float distance = 0;
  int   samples  = 0;
  int   tries    = 0;

  if (!readDistanceCm(&distance, &samples, &tries)) {
    Serial.print(F("No valid echo in "));
    Serial.print(tries);
    Serial.println(F(" pings -> showing fault"));
    filteredCm = -1.0f;   // resync filter after the echo comes back
    dispValid  = false;
    dispSamples = 0;
    showFault();
    return;
  }

  // Low-pass filter: HC-SR04 on a small bottle jitters ~+-0.4 cm, which is
  // +-2.4% on a 16 cm span and made the reading flicker 4% <-> 7%.
  // Snap instantly on a real level change, smooth small noise.
  if (filteredCm < 0.0f || fabsf(distance - filteredCm) > 4.0f) {
    filteredCm = distance;
  } else {
    filteredCm += 0.30f * (distance - filteredCm);
  }
  distance = filteredCm;

  float pct = (EMPTY_DISTANCE_CM - distance) / (EMPTY_DISTANCE_CM - FULL_DISTANCE_CM) * 100.0f;
  if (pct < 3.0f)   pct = 0.0f;   // deadband: empty-bottle noise reads a clean 0%
  if (pct > 100.0f) pct = 100.0f;
  int percent = (int)(pct + 0.5f);

  Serial.print(F("Distance: "));
  Serial.print(distance, 1);
  Serial.print(F(" cm ("));
  Serial.print(samples);
  Serial.print(F(" samples), Level: "));
  Serial.print(percent);
  Serial.print(F("%  [WiFi:"));
  Serial.print(WiFi.status() == WL_CONNECTED ? F("up") : F("down"));
  Serial.print(F(" Blynk:"));
  Serial.print(Blynk.connected() ? F("up") : F("down"));
  Serial.println(F("]"));

  dispPercent = percent;
  dispCm      = distance;
  dispSamples = samples;
  dispValid   = true;
  updateDisplay();
  handleBlynkAndBuzzer(percent);

  // Store a 1-minute average for the dashboard graph
  historyRecord(percent, distance);
}

// ---------------------------------------------------------------
//  OLED layout - everything on one 128x64 screen:
//    header   : title, inverts and blinks while the level alarm is on
//    big value: level in %
//    bar      : fill + a tick at the HIGH_LEVEL_PERCENT threshold
//    lines    : distance / samples, Wi-Fi state, Blynk state, IP
// ---------------------------------------------------------------
void updateDisplay() {
  if (!oledOk) return;

  const bool high  = dispValid && (dispPercent >= HIGH_LEVEL_PERCENT);
  const bool blink = ((millis() / 400UL) % 2UL) == 0UL;
  const bool inv   = high && blink;
  const bool wUp   = (WiFi.status() == WL_CONNECTED);
  const bool bUp   = Blynk.connected();

  display.clearDisplay();
  display.setTextSize(1);

  // --- header band ---
  if (inv) display.fillRect(0, 0, SCREEN_WIDTH, 11, SSD1306_WHITE);
  display.setTextColor(inv ? SSD1306_BLACK : SSD1306_WHITE);
  display.setCursor(2, 2);
  display.print(high ? F("!! LEVEL HIGH !!") : F("WATER LABEL"));

  // link LED, top-right: solid = Blynk up, ring = Wi-Fi only, none = offline
  display.setTextColor(SSD1306_WHITE);
  if (bUp)      display.fillCircle(122, 5, 3, SSD1306_WHITE);
  else if (wUp) display.drawCircle(122, 5, 3, SSD1306_WHITE);

  // --- big percentage ---
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(3);
  display.setCursor(0, 13);
  if (dispValid) {
    display.print(dispPercent);
    display.print('%');
  } else {
    display.print(F("--"));
  }

  // --- vertical level bar with 80% threshold tick ---
  const int barX = 104, barY = 12, barW = 20, barH = 50;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  if (dispValid) {
    int fillH = map(constrain(dispPercent, 0, 100), 0, 100, 0, barH - 2);
    if (fillH > 0) {
      display.fillRect(barX + 1, barY + barH - 1 - fillH, barW - 2, fillH, SSD1306_WHITE);
    }
  }
  int thrY = barY + barH - 1 - map(HIGH_LEVEL_PERCENT, 0, 100, 0, barH - 2);
  display.drawFastHLine(barX - 4, thrY, 4, SSD1306_WHITE);

  // --- distance + samples ---
  display.setTextSize(1);
  display.setCursor(0, 38);
  display.print(F("Dist "));
  if (dispValid) display.print(dispCm, 1);
  else           display.print(F("--.-"));
  display.print(F("cm "));
  display.print(dispSamples);
  display.print(F("/5"));

  // --- link state ---
  display.setCursor(0, 47);
  display.print(F("WiFi "));
  display.print(wUp ? F("OK") : F("--"));
  display.print(F(" Blynk "));
  display.print(bUp ? F("OK") : F("--"));

  // --- IP (or retry notice) ---
  display.setCursor(0, 56);
  if (wUp) {
    display.print(F("IP "));
    display.print(WiFi.localIP());
  } else {
    display.print(F("WiFi: reconnecting"));
  }

  display.display();
}

// Only redraws while the alarm is active, to animate the header blink.
void alertRefresh() {
  if (dispValid && dispPercent >= HIGH_LEVEL_PERCENT) updateDisplay();
}

void showFault() {
  if (!oledOk) return;

  const bool wUp = (WiFi.status() == WL_CONNECTED);
  const bool bUp = Blynk.connected();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("NO ECHO - CHECK 5V"));

  display.setTextSize(2);
  display.setCursor(0, 12);
  display.print(F("----"));

  display.setTextSize(1);
  display.setCursor(0, 34);
  display.print(F("Expect "));
  display.print(FULL_DISTANCE_CM, 1);
  display.print('-');
  display.print(EMPTY_DISTANCE_CM, 1);
  display.print(F("cm"));

  display.setCursor(0, 44);
  display.print(F("WiFi "));
  display.print(wUp ? F("OK") : F("--"));
  display.print(F(" Blynk "));
  display.print(bUp ? F("OK") : F("--"));

  display.setCursor(0, 54);
  if (wUp) {
    display.print(F("IP "));
    display.print(WiFi.localIP());
  } else {
    display.print(F("WiFi: reconnecting"));
  }

  display.display();
}

void handleBlynkAndBuzzer(int percent) {
  if (Blynk.connected()) {
    Blynk.virtualWrite(BLYNK_VPIN_LEVEL, percent);

    if (percent >= HIGH_LEVEL_PERCENT && !notified) {
      char msg[56];
      snprintf(msg, sizeof(msg), "Water level %d%%! Overflow risk.", HIGH_LEVEL_PERCENT);
      Blynk.logEvent("water_high", msg);
      notified = true;
    }
  }

  if (percent < HIGH_LEVEL_PERCENT - 5) {
    notified = false;
  }
}
// =====================================================
// CLOCK
// =====================================================

bool clockIsValid() {
  return time(nullptr) > 1700000000;
}

uint32_t minuteNow() {
  if (clockIsValid()) {
    return (uint32_t)(time(nullptr) / 60);
  }
  return (uint32_t)(millis() / 60000UL);
}

void syncClock() {
  if (WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
  }
}

void historyMaintain() {

  static unsigned long lastNtpAttempt = 0;

  // Keep trying if Wi-Fi only came up after boot
  if (!clockSynced && WiFi.status() == WL_CONNECTED &&
      millis() - lastNtpAttempt >= 30000UL) {
    lastNtpAttempt = millis();
    syncClock();
  }

  if (!clockSynced && clockIsValid()) {

    clockSynced = true;
    Serial.println(F("[Time] NTP synced"));

    // Relative-time samples are useless once real time is known
    if (histSynthetic) {
      historyClear();
      histSynthetic = false;
    }
  }

  if (histDirty && millis() - lastHistorySave >= HISTORY_SAVE_INTERVAL_MS) {
    historySave();
  }
}

// =====================================================
// HISTORY RING BUFFER (1-MINUTE AVERAGES)
// =====================================================

void historyAppend(uint32_t minute, uint8_t pct, uint8_t cm) {

  histMinute[histHead] = minute;
  histPct[histHead] = pct;
  histCm[histHead] = cm;

  histHead = (uint16_t)((histHead + 1) % HISTORY_CAPACITY);

  if (histCount < HISTORY_CAPACITY) {
    histCount++;
  }

  histDirty = true;
}

void historyFlushMinute() {

  if (!aggActive || aggCount == 0) {
    aggActive = false;
    aggCount = 0;
    return;
  }

  uint32_t pct = (aggSumPct + aggCount / 2) / aggCount;
  if (pct > 100) pct = 100;

  float cmAvg = (float)aggSumCm10 / 10.0f / (float)aggCount;

  uint32_t cm = (uint32_t)(cmAvg + 0.5f);
  if (cm > 255) cm = 255;

  historyAppend(aggMinute, (uint8_t)pct, (uint8_t)cm);

  aggActive = false;
  aggCount = 0;
}

void historyRecord(int percent, float cm) {

  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  const bool haveClock = clockIsValid();

  // A store written with real timestamps must never receive relative ones:
  // mixing the two hides the old points and the sync handler would then throw
  // the whole file away. Wait for NTP instead.
  if (!haveClock && !histSynthetic && histCount > 0) {
    return;
  }

  // First reading after NTP: the stored points are relative and cannot be
  // mapped onto real time, so drop them now. This is the only safe moment to
  // clear, because the store is guaranteed to contain only relative points.
  if (haveClock && histSynthetic) {
    historyClear();
    histSynthetic = false;
  }

  uint32_t m = minuteNow();

  if (!aggActive || m != aggMinute) {

    historyFlushMinute();

    aggActive = true;
    aggMinute = m;
    aggSumPct = 0;
    aggSumCm10 = 0;
    aggCount = 0;
  }

  if (!haveClock) histSynthetic = true;

  aggSumPct += (uint32_t)percent;
  aggSumCm10 += (uint32_t)(cm * 10.0f + 0.5f);

  if (aggCount < 60000) aggCount++;
}

void historyClear() {

  histHead = 0;
  histCount = 0;
  aggActive = false;
  aggCount = 0;
  histDirty = false;

  if (fsReady) {
    LittleFS.remove(HISTORY_FILE);
  }

  Serial.println(F("[History] Cleared"));
}

void historySave() {

  // Stamp every attempt, not just successful ones: a full flash or a failing
  // LittleFS mount must not make historyMaintain() retry on every loop pass.
  lastHistorySave = millis();

  if (!fsReady) return;

  File f = LittleFS.open(HISTORY_FILE, "w");

  if (!f) {
    Serial.println(F("[History] Save failed"));
    return;
  }

  HistoryHeader h;
  h.magic = HISTORY_MAGIC;
  h.count = histCount;
  h.head = histHead;
  h.synthetic = histSynthetic ? 1 : 0;
  memset(h.reserved, 0, sizeof(h.reserved));

  const size_t want = sizeof(h) + sizeof(histMinute) + sizeof(histPct) + sizeof(histCm);
  size_t got = 0;
  got += f.write((const uint8_t*)&h, sizeof(h));
  got += f.write((const uint8_t*)histMinute, sizeof(histMinute));
  got += f.write((const uint8_t*)histPct, sizeof(histPct));
  got += f.write((const uint8_t*)histCm, sizeof(histCm));
  f.close();

  // A short write means the file is unusable; keep histDirty set so the next
  // interval retries instead of pretending the snapshot was saved.
  if (got != want) {
    Serial.println(F("[History] Short write - keeping data dirty"));
    return;
  }

  histDirty = false;

  Serial.print(F("[History] Saved "));
  Serial.print(histCount);
  Serial.println(F(" points"));
}

void historyLoad() {

  fsReady = LittleFS.begin(true);

  if (!fsReady) {
    Serial.println(F("[History] LittleFS unavailable"));
    return;
  }

  File f = LittleFS.open(HISTORY_FILE, "r");

  if (!f) {
    Serial.println(F("[History] No saved history yet"));
    return;
  }

  HistoryHeader h;

  bool ok =
    f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) &&
    h.magic == HISTORY_MAGIC &&
    h.count <= HISTORY_CAPACITY &&
    h.head < HISTORY_CAPACITY;

  if (ok) {
    ok =
      f.read((uint8_t*)histMinute, sizeof(histMinute)) == sizeof(histMinute) &&
      f.read((uint8_t*)histPct, sizeof(histPct)) == sizeof(histPct) &&
      f.read((uint8_t*)histCm, sizeof(histCm)) == sizeof(histCm);
  }

  f.close();

  if (!ok) {
    Serial.println(F("[History] Invalid file, starting fresh"));
    LittleFS.remove(HISTORY_FILE);
    return;
  }

  histCount = h.count;
  histHead = h.head;
  histSynthetic = (h.synthetic != 0);

  // Older firmware set the synthetic flag as soon as one reading happened
  // before NTP, even when the file already held a week of real timestamps.
  // Trust the data over the flag: if any point carries a real epoch minute,
  // keep the store as real so the first sync after an update cannot wipe it.
  if (histSynthetic && histCount > 0) {
    uint16_t start = (uint16_t)((histHead + HISTORY_CAPACITY - histCount) % HISTORY_CAPACITY);
    for (uint16_t i = 0; i < histCount; i++) {
      uint16_t idx = (uint16_t)((start + i) % HISTORY_CAPACITY);
      if (histMinute[idx] > (1700000000UL / 60UL)) {
        histSynthetic = false;
        break;
      }
    }
  }

  Serial.print(F("[History] Loaded "));
  Serial.print(histCount);
  Serial.println(F(" points"));
}

// =====================================================
// WEB SERVER
// =====================================================

void setupWebDashboard() {

  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/api/live", HTTP_GET, handleLive);
  webServer.on("/api/history", HTTP_GET, handleHistory);
  webServer.on("/api/export.csv", HTTP_GET, handleExportCsv);
  webServer.on("/api/clear", HTTP_POST, handleClearHistory);

  webServer.on("/favicon.ico", HTTP_GET, []() {
    webServer.send(204, "text/plain", "");
  });

  webServer.onNotFound([]() {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
  });

  webServer.begin();
  webStarted = true;

  Serial.println(F("[Web] Server started on port 80"));
}

void handleRoot() {
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

void handleLive() {

  String json;
  json.reserve(340);

  json += "{\"ok\":true";

  json += ",\"percent\":";
  json += dispValid ? dispPercent : -1;

  json += ",\"cm\":";
  json += String(dispValid ? dispCm : 0.0f, 1);

  json += ",\"samples\":";
  json += dispSamples;

  json += ",\"sensor\":";
  json += dispValid ? "true" : "false";

  json += ",\"wifi\":";
  json += (WiFi.status() == WL_CONNECTED) ? "true" : "false";

  json += ",\"blynk\":";
  json += Blynk.connected() ? "true" : "false";

  json += ",\"rssi\":";
  json += WiFi.RSSI();

  json += ",\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\"";

  json += ",\"uptime\":";
  json += (unsigned long)(millis() / 1000UL);

  json += ",\"time\":";
  json += (unsigned long)time(nullptr);

  json += ",\"timeValid\":";
  json += clockIsValid() ? "true" : "false";

  json += ",\"high\":";
  json += HIGH_LEVEL_PERCENT;

  json += ",\"alert\":";
  json += (dispValid && dispPercent >= HIGH_LEVEL_PERCENT) ? "true" : "false";

  json += ",\"history\":";
  json += histCount;

  json += ",\"capacity\":";
  json += HISTORY_CAPACITY;

  json += "}";

  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json", json);
}

void handleHistory() {

  int hours = 24;
  if (webServer.hasArg("hours")) {
    hours = webServer.arg("hours").toInt();
  }
  if (hours < 1) hours = 1;
  if (hours > 168) hours = 168;

  int maxPoints = 600;
  if (webServer.hasArg("points")) {
    maxPoints = webServer.arg("points").toInt();
  }
  if (maxPoints < 60) maxPoints = 60;
  if (maxPoints > 1200) maxPoints = 1200;

  uint32_t nowMin = minuteNow();

  // A reboot without NTP leaves the clock invalid while the store still holds
  // real timestamps, so minuteNow() is a tiny millis-based value and every
  // saved point is filtered out ("m > nowMin") - the graph went blank for as
  // long as NTP stayed away, even though the week was intact. Anchor the
  // window on the newest saved point, exactly like the CSV export does.
  if (!clockIsValid() && !histSynthetic && histCount > 0) {
    nowMin = histMinute[(histHead + HISTORY_CAPACITY - 1) % HISTORY_CAPACITY];
  }

  uint32_t windowMin = (uint32_t)hours * 60UL;
  uint32_t fromMin = (nowMin > windowMin) ? (nowMin - windowMin) : 0;

  uint32_t span = nowMin - fromMin;
  if (span == 0) span = 1;

  uint32_t step = (span + (uint32_t)maxPoints - 1) / (uint32_t)maxPoints;
  if (step < 1) step = 1;

  webServer.sendHeader("Cache-Control", "no-store");
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "application/json", "");

  String buf;
  buf.reserve(1200);

  buf += "{\"ok\":true,\"synthetic\":";
  buf += histSynthetic ? "true" : "false";
  buf += ",\"nowMinute\":";
  buf += nowMin;
  buf += ",\"step\":";
  buf += step;
  buf += ",\"hours\":";
  buf += hours;
  buf += ",\"points\":[";

  bool first = true;

  bool haveBucket = false;
  uint32_t bucketId = 0;
  uint32_t bucketStart = 0;
  uint32_t sumPct = 0;
  uint32_t sumCm = 0;
  uint16_t n = 0;
  uint8_t minPct = 255;
  uint8_t maxPct = 0;

  auto appendPoint = [&]() {

    if (n == 0) return;

    char line[96];

    snprintf(
      line,
      sizeof(line),
      "%s[%lu,%u,%u,%u,%.1f]",
      first ? "" : ",",
      (unsigned long)(bucketStart * 60UL),
      (unsigned)((sumPct + n / 2) / n),
      (unsigned)minPct,
      (unsigned)maxPct,
      (double)((float)sumCm / (float)n)
    );

    first = false;
    buf += line;

    if (buf.length() > 1024) {
      webServer.sendContent(buf);
      buf = "";
    }
  };

  uint16_t start =
    (uint16_t)((histHead + HISTORY_CAPACITY - histCount) % HISTORY_CAPACITY);

  for (uint16_t i = 0; i < histCount; i++) {

    uint16_t idx = (uint16_t)((start + i) % HISTORY_CAPACITY);
    uint32_t m = histMinute[idx];

    if (m < fromMin || m > nowMin) {
      continue;
    }

    uint32_t id = (m - fromMin) / step;

    if (!haveBucket || id != bucketId) {

      appendPoint();

      haveBucket = true;
      bucketId = id;
      bucketStart = fromMin + id * step;
      sumPct = 0;
      sumCm = 0;
      n = 0;
      minPct = 255;
      maxPct = 0;
    }

    sumPct += histPct[idx];
    sumCm += histCm[idx];
    n++;

    if (histPct[idx] < minPct) minPct = histPct[idx];
    if (histPct[idx] > maxPct) maxPct = histPct[idx];
  }

  appendPoint();

  buf += "]}";

  webServer.sendContent(buf);
  webServer.sendContent("");
}

// One row per stored minute, newest last. Streamed in chunks so a 7-day
// export (~10k rows) does not need a big buffer.
void handleExportCsv() {

  int hours = 168;
  if (webServer.hasArg("hours")) {
    hours = webServer.arg("hours").toInt();
  }
  if (hours < 1) hours = 1;
  if (hours > 168) hours = 168;

  uint32_t nowMin = minuteNow();

  // With no clock but a store that already has real timestamps, anchor the
  // window on the newest saved point instead of returning an empty file.
  if (!clockIsValid() && !histSynthetic && histCount > 0) {
    nowMin = histMinute[(histHead + HISTORY_CAPACITY - 1) % HISTORY_CAPACITY];
  }

  uint32_t windowMin = (uint32_t)hours * 60UL;
  uint32_t fromMin = (nowMin > windowMin) ? (nowMin - windowMin) : 0;

  webServer.sendHeader("Cache-Control", "no-store");
  webServer.sendHeader("Content-Disposition", "attachment; filename=\"water-level.csv\"");
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/csv; charset=utf-8", "");

  String buf;
  buf.reserve(1024);
  buf += "time_base,epoch_seconds,utc_time,level_percent,distance_cm\r\n";

  uint16_t start =
    (uint16_t)((histHead + HISTORY_CAPACITY - histCount) % HISTORY_CAPACITY);

  for (uint16_t i = 0; i < histCount; i++) {

    uint16_t idx = (uint16_t)((start + i) % HISTORY_CAPACITY);
    uint32_t m = histMinute[idx];

    if (m < fromMin || m > nowMin) {
      continue;
    }

    char utc[24] = "";

    if (!histSynthetic) {
      time_t t = (time_t)m * 60;
      struct tm tmv;
      if (gmtime_r(&t, &tmv) != nullptr) {
        strftime(utc, sizeof(utc), "%Y-%m-%dT%H:%M:%SZ", &tmv);
      }
    }

    char line[96];
    snprintf(
      line,
      sizeof(line),
      "%s,%lu,%s,%u,%u\r\n",
      histSynthetic ? "relative" : "epoch",
      (unsigned long)(m * 60UL),
      utc,
      (unsigned)histPct[idx],
      (unsigned)histCm[idx]
    );

    buf += line;

    if (buf.length() > 900) {
      webServer.sendContent(buf);
      buf = "";
    }
  }

  webServer.sendContent(buf);
  webServer.sendContent("");
}

void handleClearHistory() {
  historyClear();
  webServer.send(200, "application/json", "{\"ok\":true}");
}
