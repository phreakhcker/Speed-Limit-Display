// =============================================
// SPEED LIMIT DISPLAY — Rev 4
// =============================================
// ESP32 Mini + GPS + SSD1351 OLED + TomTom API
//
// What this does:
//   - Reads your GPS position and heading
//   - Asks TomTom "what road am I on and what's the speed limit?"
//   - Shows the speed limit on the OLED display
//   - Changes color: GREEN (ok), YELLOW (close to limit), RED (over limit)
//
// What's new in Rev4 (vs Rev3):
//   - Uses TomTom "Snap to Roads" API instead of Reverse Geocode
//     → sends your heading and speed so TomTom picks the RIGHT road
//   - Buffers multiple GPS points for better road matching
//   - Speed limit smoothing — rejects wild jumps (side road false matches)
//   - GPS baud rate increased to 38400 (supports 10Hz without data loss)
//   - Non-blocking WiFi (GPS keeps working during WiFi reconnects)
//   - Low-speed dead zone (no false alerts when parked)
//   - Watchdog timer (auto-reboots if device freezes)
//   - Credentials in separate file (config_secrets.h)
//   - All tunable settings in config.h
//
// Hardware:
//   - ESP32 Mini (any variant: S2, S3, C3, or original)
//   - GPS module (UART, any NMEA-compatible like NEO-6M, NEO-7M, BN-220)
//   - SSD1351 128x128 SPI OLED display
//   - One pushbutton (between BTN_PIN and GND)
//
// Libraries needed (install via Arduino Library Manager):
//   - TinyGPSPlus (by Mikal Hart)
//   - ArduinoJson (by Benoit Blanchon, version 6.x)
//   - Adafruit GFX Library
//   - Adafruit SSD1351 Library
// =============================================

// IMPORTANT: This enum must be BEFORE includes so Arduino's
// auto-prototype generator doesn't break it.
// (Arduino IDE automatically creates function declarations at the top of your
//  code, but it can mess up if enums are defined after includes.)
enum BtnEvent : uint8_t { BTN_NONE, BTN_SHORT, BTN_LONG, BTN_VLONG };

// --- Standard Libraries ---
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>         // NVS = Non-Volatile Storage (survives reboots)
#include <esp_task_wdt.h>        // Watchdog timer

// --- Hardware Libraries ---
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

// --- Fonts ---
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

// --- Our Config Files ---
#include "config.h"              // Pin definitions, timing, thresholds
#include "config_secrets.h"      // WiFi password, API key (git-ignored)


// =============================================================
// SECTION 1: HARDWARE OBJECTS
// =============================================================
// These are the "objects" that represent your physical hardware.
// Think of them as software handles — you call methods on them
// to control the actual hardware.

// The OLED display. We tell it the screen size, which SPI bus, and which pins.
Adafruit_SSD1351 display(SCREEN_W, SCREEN_H, &SPI, OLED_CS, OLED_DC, OLED_RST);

// The GPS parser. It reads raw NMEA sentences (text from the GPS module)
// and turns them into useful data (latitude, longitude, speed, heading, etc.)
TinyGPSPlus gps;

// Hardware serial port #1 for GPS communication.
// ESP32 has multiple serial ports — Serial (USB) is #0, this is #1.
HardwareSerial GPSSerial(1);

// Non-volatile preferences storage (saves settings when power is off).
Preferences prefs;


// =============================================================
// SECTION 2: STATE VARIABLES
// =============================================================
// These track what's happening right now. They live in RAM and
// reset to their initial values every time the device reboots.

// --- GPS state ---
uint32_t bootMs       = 0;       // When the device started (millis timestamp)
uint32_t lastByteMs   = 0;       // Last time we received ANY data from GPS
uint32_t lastFixMs    = 0;       // Last time GPS had a valid position fix
uint32_t firstFixMs   = 0;       // First-ever valid fix (0 = never had one)
bool     haveFixEver  = false;   // Have we EVER gotten a GPS fix this session?

// --- Speed limit state ---
int      currentSpeedLimitMph = -1;   // Currently displayed speed limit (-1 = unknown)
uint32_t lastGoodLimitMs      = 0;    // When we last got a valid speed limit

// --- API call state ---
uint32_t lastApiMs     = 0;      // When we last called the TomTom API
uint32_t lastApiFailMs = 0;      // When the API last failed (for warning icon)
double   lastApiLat    = 0;      // GPS position of last API call
double   lastApiLon    = 0;
bool     haveApiPos    = false;  // Have we made at least one API call?

// --- User settings (saved to NVS) ---
int      brightIdx         = 0;       // Current brightness level index
float    brightnessScale   = 1.0f;    // Actual brightness multiplier (0.0-1.0)
int      yellowOverMph     = 3;       // Show yellow when this many mph over limit
int      redOverMph        = 4;       // Show red when this many mph over limit
uint32_t apiMinIntervalMs  = API_DEFAULT_INTERVAL_MS;
float    apiMinMoveM       = API_DEFAULT_MOVE_M;

// --- Display cache ---
// We only redraw the screen when something changes. These track what's
// currently shown so we can skip unnecessary redraws (which are slow on SPI).
String   lastMainText      = "";
uint16_t lastMainColor     = 0xFFFF;
int      lastShownSats     = -999;
bool     lastGpsIconSolid  = false;
int      lastWifiBars      = -1;
bool     lastApiWarn       = false;

// --- Button state ---
bool     btnLastRead     = true;    // Last raw reading (HIGH = not pressed)
bool     btnStable       = true;    // Debounced stable state
uint32_t btnLastChangeMs = 0;       // When the raw reading last changed
uint32_t btnPressStartMs = 0;       // When current press started
bool     btnIsDown       = false;   // Is button currently held down?

// --- Menu state ---
bool menuActive = false;
int  menuIndex  = 0;
bool menuDirty  = true;             // Does the menu need to be redrawn?

// --- Overspeed state machine ---
// Uses hysteresis to prevent flickering between colors at the boundary.
// "Hysteresis" means it takes a slightly BIGGER change to switch states
// than it does to stay in the current state. Like a thermostat that turns
// on the heater at 68F but doesn't turn it off until 72F.
enum OverState : uint8_t { OK_GREEN, WARN_YELLOW, OVER_RED };
OverState overState = OK_GREEN;

// --- WiFi state (non-blocking) ---
bool     wifiConnecting    = false;   // Are we in the middle of connecting?
uint32_t wifiConnectStart  = 0;       // When we started the current connection attempt
uint32_t wifiLastAttemptMs = 0;       // When we last tried to connect

// --- GPS point buffer for Snap-to-Roads ---
// We store the last few GPS readings so we can send them all to TomTom.
// Multiple points help TomTom figure out which road you're actually on
// by looking at your trajectory (path of travel), not just one position.
struct GpsPoint {
  double   lat;
  double   lon;
  float    heading;     // Direction of travel in degrees (0=North, 90=East, etc.)
  float    speedKmh;    // Speed in km/h (TomTom expects metric)
  uint32_t timestamp;   // millis() when this point was captured
  bool     valid;       // Is this point filled in yet?
};

GpsPoint gpsBuffer[GPS_BUFFER_SIZE];
int      gpsBufferIndex = 0;   // Next slot to write into (circular buffer)
uint32_t lastBufferMs   = 0;   // When we last added a point to the buffer

// --- Speed limit smoothing ---
// Keeps a history of recent API results to detect and reject outliers.
// An "outlier" is a single reading that's wildly different from recent values —
// usually caused by GPS matching a nearby road by mistake.
int  limitHistory[LIMIT_HISTORY_SIZE];
int  limitHistoryCount = 0;       // How many values are in the history
int  pendingLimit      = -1;      // A "suspicious" new limit waiting for confirmation
int  pendingConfirms   = 0;       // How many times we've seen the pending limit


// =============================================================
// SECTION 3: COLORS (RGB565)
// =============================================================
// The SSD1351 display uses "RGB565" color format:
//   - 5 bits for Red (0-31)
//   - 6 bits for Green (0-63) — human eyes are more sensitive to green
//   - 5 bits for Blue (0-31)
// These are packed into a single 16-bit number.

static inline uint16_t C_BLACK()  { return 0x0000; }
static inline uint16_t C_WHITE()  { return 0xFFFF; }
static inline uint16_t C_GRAY()   { return 0x8410; }
static inline uint16_t C_GREEN()  { return 0x07E0; }
static inline uint16_t C_YELLOW() { return 0xFFE0; }
static inline uint16_t C_RED()    { return 0xF800; }

// Apply software brightness dimming to a color.
// We multiply each color channel by the brightness scale factor.
uint16_t scaleColor(uint16_t color) {
  if (color == C_BLACK()) return color;   // Black stays black regardless

  // Extract the individual R, G, B channels from the packed 16-bit value
  uint8_t r = (color >> 11) & 0x1F;   // Top 5 bits
  uint8_t g = (color >> 5)  & 0x3F;   // Middle 6 bits
  uint8_t b =  color        & 0x1F;   // Bottom 5 bits

  // Scale each channel by brightness
  r = (uint8_t)(r * brightnessScale);
  g = (uint8_t)(g * brightnessScale);
  b = (uint8_t)(b * brightnessScale);

  // Clamp to max values (shouldn't exceed, but safety first)
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (b > 31) b = 31;

  // Pack back into RGB565
  return (r << 11) | (g << 5) | b;
}


// =============================================================
// SECTION 4: SERIAL COMMANDS
// =============================================================
// You can type commands over USB Serial (the Arduino Serial Monitor).
// This is useful for debugging — type 'r' to reboot, 'd' for debug info.

void checkSerialCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'r' || c == 'R') {
      Serial.println("[CMD] Rebooting...");
      delay(100);
      ESP.restart();
    }
    else if (c == 'd' || c == 'D') {
      // Print current state for debugging
      Serial.println("=== DEBUG INFO ===");
      Serial.printf("  GPS fix: %s | Sats: %d\n",
        gps.location.isValid() ? "YES" : "NO",
        gps.satellites.isValid() ? gps.satellites.value() : 0);
      Serial.printf("  Lat: %.6f  Lon: %.6f\n",
        gps.location.isValid() ? gps.location.lat() : 0.0,
        gps.location.isValid() ? gps.location.lng() : 0.0);
      Serial.printf("  Heading: %.1f°  Speed: %.1f mph\n",
        gps.course.isValid() ? gps.course.deg() : 0.0,
        gps.speed.isValid() ? gps.speed.mph() : 0.0);
      Serial.printf("  WiFi: %s (RSSI: %d)\n",
        WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected",
        WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
      Serial.printf("  Speed limit: %d mph\n", currentSpeedLimitMph);
      Serial.printf("  API calls since boot: interval=%lums, move=%.0fm\n",
        apiMinIntervalMs, apiMinMoveM);
      Serial.printf("  Brightness: level %d/%d (%.0f%%)\n",
        brightIdx + 1, BRIGHT_LEVEL_COUNT, brightnessScale * 100);
      Serial.printf("  Buffer points: %d/%d\n",
        min(limitHistoryCount, LIMIT_HISTORY_SIZE), LIMIT_HISTORY_SIZE);
      Serial.println("==================");
    }
  }
}


// =============================================================
// SECTION 5: PREFERENCES (Persistent Settings)
// =============================================================
// NVS (Non-Volatile Storage) is flash memory on the ESP32 that
// survives reboots and power cycles. We use it to remember your
// brightness level, alert thresholds, etc.

void saveSettings() {
  prefs.putUChar("bri", (uint8_t)brightIdx);
  prefs.putUChar("yov", (uint8_t)yellowOverMph);
  prefs.putUChar("rov", (uint8_t)redOverMph);
  prefs.putUInt("apiI", apiMinIntervalMs);
  prefs.putUInt("apiM", (uint32_t)lround(apiMinMoveM));
}

void loadSettings() {
  brightIdx = prefs.getUChar("bri", 0);
  if (brightIdx >= BRIGHT_LEVEL_COUNT) brightIdx = 0;
  brightnessScale = BRIGHT_LEVELS[brightIdx];

  yellowOverMph = prefs.getUChar("yov", 3);
  redOverMph    = prefs.getUChar("rov", 4);
  if (yellowOverMph < 1)             yellowOverMph = 3;
  if (redOverMph <= yellowOverMph)   redOverMph = yellowOverMph + 1;

  apiMinIntervalMs = prefs.getUInt("apiI", API_DEFAULT_INTERVAL_MS);
  uint32_t mm = prefs.getUInt("apiM", (uint32_t)API_DEFAULT_MOVE_M);
  apiMinMoveM = (float)mm;

  // Sanity checks — clamp to reasonable ranges
  if (apiMinIntervalMs < 2000)  apiMinIntervalMs = 2000;
  if (apiMinIntervalMs > 30000) apiMinIntervalMs = 30000;
  if (apiMinMoveM < 10)  apiMinMoveM = 10;
  if (apiMinMoveM > 300) apiMinMoveM = 300;
}

void applyBrightness() {
  brightnessScale = BRIGHT_LEVELS[brightIdx];
  saveSettings();

  // Force a full screen redraw by resetting all cache values
  lastMainText = "";
  lastShownSats = -999;
  lastWifiBars = -1;
  lastApiWarn = !lastApiWarn;
  menuDirty = true;
}

void nextBrightnessLevel() {
  brightIdx = (brightIdx + 1) % BRIGHT_LEVEL_COUNT;
  applyBrightness();
}


// =============================================================
// SECTION 6: BUTTON EVENT POLLER
// =============================================================
// Reads the button and returns what kind of press happened.
// Uses "debouncing" to filter out electrical noise.
//
// How it works:
//   1. Read the pin (HIGH = not pressed, LOW = pressed, because INPUT_PULLUP)
//   2. If the reading changed, start a debounce timer
//   3. Only accept the new state after it's been stable for BTN_DEBOUNCE_MS
//   4. When released, measure how long it was held to determine press type

BtnEvent pollButtonEvent() {
  bool reading = (digitalRead(BTN_PIN) == HIGH);   // HIGH = not pressed

  // If reading changed, reset the debounce timer
  if (reading != btnLastRead) {
    btnLastRead = reading;
    btnLastChangeMs = millis();
  }

  // Still bouncing? Wait longer.
  if ((millis() - btnLastChangeMs) < BTN_DEBOUNCE_MS) return BTN_NONE;

  // State has been stable long enough — accept it
  if (btnStable != btnLastRead) {
    btnStable = btnLastRead;

    if (btnStable == false) {
      // Button was just pressed DOWN
      btnIsDown = true;
      btnPressStartMs = millis();
      return BTN_NONE;   // We report the event on RELEASE, not press
    } else {
      // Button was just RELEASED — determine press type by duration
      if (!btnIsDown) return BTN_NONE;
      btnIsDown = false;

      uint32_t held = millis() - btnPressStartMs;
      if (held >= BTN_VLONG_MS) return BTN_VLONG;    // Very long press
      if (held >= BTN_LONG_MS)  return BTN_LONG;      // Long press
      return BTN_SHORT;                                // Short press
    }
  }

  return BTN_NONE;
}


// =============================================================
// SECTION 7: GPS FUNCTIONS
// =============================================================

// Calculate NMEA checksum (XOR of all characters between $ and *)
uint8_t nmeaChecksum(const char* s) {
  uint8_t cs = 0;
  for (; *s; s++) cs ^= (uint8_t)(*s);
  return cs;
}

// Send a PMTK command to the GPS module.
// PMTK = "MTK Proprietary NMEA sentence" — commands specific to MediaTek GPS chips
// (which are used in most cheap GPS modules like NEO-6M, BN-220, etc.)
void gpsSendPMTK(const char* body) {
  char buf[96];
  uint8_t cs = nmeaChecksum(body);
  snprintf(buf, sizeof(buf), "$%s*%02X\r\n", body, cs);
  GPSSerial.print(buf);
}

// Configure GPS for our use case:
//   - RMC only: reduces serial traffic (we only need position, speed, heading)
//   - 10Hz: updates 10 times per second (smooth speed readings)
//   - Vehicle mode: optimizes GPS filtering for car movement
void gpsConfigureHighRate() {
  // First: configure at default 9600 baud
  // RMC-only output (turn off all other sentence types)
  gpsSendPMTK("PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
  delay(50);

  // Set 10Hz update rate (100ms between fixes)
  gpsSendPMTK("PMTK220,100");
  delay(50);

  // Vehicle dynamics mode (better filtering for car movement)
  gpsSendPMTK("PMTK886,0");
  delay(50);

  // Change GPS module baud rate to 38400
  // After this command, the GPS will start talking at 38400 instead of 9600.
  gpsSendPMTK("PMTK251,38400");
  delay(100);   // Give the GPS time to switch

  // Now switch OUR serial port to match
  GPSSerial.end();
  delay(50);
  GPSSerial.begin(GPS_TARGET_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(100);

  Serial.printf("[GPS] Configured: 10Hz, RMC-only, vehicle mode, %lu baud\n", GPS_TARGET_BAUD);
}

// Warm restart — GPS keeps its almanac/ephemeris data but re-acquires satellites.
// Faster than a cold restart. Useful if GPS seems stuck or inaccurate.
void gpsWarmRestart() {
  gpsSendPMTK("PMTK102");
  Serial.println("[GPS] Warm restart sent");
}

// Add the current GPS reading to our point buffer.
// This is a "circular buffer" — when it fills up, the oldest point gets overwritten.
// Think of it like a conveyor belt: new points go on one end, old ones fall off the other.
void bufferGpsPoint() {
  if (!gps.location.isValid() || !gps.course.isValid()) return;

  GpsPoint& p = gpsBuffer[gpsBufferIndex];
  p.lat       = gps.location.lat();
  p.lon       = gps.location.lng();
  p.heading   = gps.course.isValid() ? gps.course.deg() : 0;
  p.speedKmh  = gps.speed.isValid() ? gps.speed.kmph() : 0;
  p.timestamp = millis();
  p.valid     = true;

  gpsBufferIndex = (gpsBufferIndex + 1) % GPS_BUFFER_SIZE;
  lastBufferMs = millis();
}


// =============================================================
// SECTION 8: WIFI (Non-Blocking)
// =============================================================
// "Non-blocking" means we don't freeze the device waiting for WiFi.
// Instead, we start the connection and check back each loop iteration.
// Meanwhile, GPS keeps running, the display keeps updating, etc.

// Call this every loop iteration. It manages WiFi without blocking.
// Returns true if WiFi is currently connected and ready to use.
bool wifiManage() {
  // Already connected? Great, nothing to do.
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnecting = false;
    return true;
  }

  // Are we currently trying to connect?
  if (wifiConnecting) {
    // Check if we've been trying too long
    if ((millis() - wifiConnectStart) > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println("[WiFi] Connection attempt timed out");
      WiFi.disconnect();
      wifiConnecting = false;
      wifiLastAttemptMs = millis();
    }
    // Still waiting — don't block, just return and try again next loop
    return false;
  }

  // Not connected and not trying — should we start a new attempt?
  if ((millis() - wifiLastAttemptMs) >= WIFI_RETRY_INTERVAL_MS) {
    Serial.println("[WiFi] Starting connection attempt...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    wifiConnecting = true;
    wifiConnectStart = millis();
  }

  return false;
}


// =============================================================
// SECTION 9: HELPER FUNCTIONS
// =============================================================

// Calculate the distance between two GPS coordinates in meters.
// Uses the "Haversine formula" — accounts for Earth being round.
double distanceMeters(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0;   // Earth's radius in meters
  double p1 = lat1 * DEG_TO_RAD;
  double p2 = lat2 * DEG_TO_RAD;
  double dp = (lat2 - lat1) * DEG_TO_RAD;
  double dl = (lon2 - lon1) * DEG_TO_RAD;

  double a = sin(dp/2)*sin(dp/2) + cos(p1)*cos(p2)*sin(dl/2)*sin(dl/2);
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

// Overspeed state machine with hysteresis.
// The "enter" threshold is slightly higher than the "exit" threshold.
// This prevents flickering when your speed is right at the boundary.
OverState computeOverState(int speedMph, int limitMph) {
  int over = speedMph - limitMph;

  int Y_ENTER = yellowOverMph;
  int R_ENTER = redOverMph;
  int Y_EXIT  = max(0, yellowOverMph - 1);
  int R_EXIT  = max(Y_ENTER, redOverMph - 1);

  switch (overState) {
    case OK_GREEN:
      if (over >= R_ENTER) return OVER_RED;
      if (over >= Y_ENTER) return WARN_YELLOW;
      return OK_GREEN;
    case WARN_YELLOW:
      if (over >= R_ENTER) return OVER_RED;
      if (over <= Y_EXIT)  return OK_GREEN;
      return WARN_YELLOW;
    case OVER_RED:
      if (over <= R_EXIT) return WARN_YELLOW;
      return OVER_RED;
  }
  return OK_GREEN;
}

uint16_t colorForOverState(OverState s) {
  if (s == OVER_RED)    return C_RED();
  if (s == WARN_YELLOW) return C_YELLOW();
  return C_GREEN();
}


// =============================================================
// SECTION 10: SPEED LIMIT SMOOTHING
// =============================================================
// The API sometimes returns wrong speed limits (matching a nearby road).
// Smoothing rejects these "outliers" by requiring confirmation.
//
// How it works:
//   1. If the new limit is close to the current one (within threshold), accept it
//   2. If it's a big jump, put it in "pending" and wait for another reading
//   3. If we see the same big-jump value multiple times in a row, accept it
//      (it's probably real — you actually turned onto a different road)

// Add a raw API result to the history and return the smoothed value.
int smoothSpeedLimit(int rawLimit) {
  if (rawLimit <= 0) return currentSpeedLimitMph;   // Bad reading, keep current

  // First ever reading? Accept it immediately.
  if (currentSpeedLimitMph <= 0) {
    pendingLimit = -1;
    pendingConfirms = 0;
    return rawLimit;
  }

  int diff = abs(rawLimit - currentSpeedLimitMph);

  // Small change (within threshold)? Accept immediately.
  // This handles normal speed limit changes (55 → 45) that happen gradually.
  if (diff <= LIMIT_JUMP_THRESHOLD_MPH) {
    pendingLimit = -1;
    pendingConfirms = 0;

    // Add to history
    if (limitHistoryCount < LIMIT_HISTORY_SIZE) {
      limitHistory[limitHistoryCount++] = rawLimit;
    } else {
      // Shift history left and add new value at the end
      for (int i = 0; i < LIMIT_HISTORY_SIZE - 1; i++) {
        limitHistory[i] = limitHistory[i + 1];
      }
      limitHistory[LIMIT_HISTORY_SIZE - 1] = rawLimit;
    }

    return rawLimit;
  }

  // Big jump! This might be a GPS glitch or a real road change.
  // Is this the same pending value we saw before?
  if (rawLimit == pendingLimit) {
    pendingConfirms++;
    Serial.printf("[SMOOTH] Pending limit %d confirmed %d/%d times\n",
                  pendingLimit, pendingConfirms, LIMIT_CONFIRM_COUNT);

    if (pendingConfirms >= LIMIT_CONFIRM_COUNT) {
      // Confirmed! This is a real speed limit change.
      Serial.printf("[SMOOTH] Accepted new limit: %d → %d mph\n",
                    currentSpeedLimitMph, rawLimit);
      pendingLimit = -1;
      pendingConfirms = 0;
      return rawLimit;
    }
  } else {
    // Different value than what was pending — start over
    pendingLimit = rawLimit;
    pendingConfirms = 1;
    Serial.printf("[SMOOTH] New pending limit: %d (current: %d, diff: %d)\n",
                  rawLimit, currentSpeedLimitMph, diff);
  }

  // Not confirmed yet — keep the current value
  return currentSpeedLimitMph;
}


// =============================================================
// SECTION 11: TOMTOM API
// =============================================================

// --- Snap to Roads (PRIMARY — more accurate) ---
// This endpoint takes GPS points with heading and speed, and matches
// them to the actual road network. Much better than Reverse Geocode
// because it knows WHICH DIRECTION you're going.

int fetchSpeedLimitSnapToRoads(double lat, double lon) {
  // Build the JSON request body with our buffered GPS points
  String body = "{\"points\":[";
  int pointsAdded = 0;

  // Add buffered points (oldest first)
  for (int i = 0; i < GPS_BUFFER_SIZE; i++) {
    int idx = (gpsBufferIndex + i) % GPS_BUFFER_SIZE;
    if (!gpsBuffer[idx].valid) continue;

    // Skip points older than 30 seconds (they're too stale to help)
    if ((millis() - gpsBuffer[idx].timestamp) > 30000) continue;

    if (pointsAdded > 0) body += ",";
    body += "{\"latitude\":";
    body += String(gpsBuffer[idx].lat, 6);
    body += ",\"longitude\":";
    body += String(gpsBuffer[idx].lon, 6);
    body += ",\"heading\":";
    body += String((int)gpsBuffer[idx].heading);
    body += ",\"speed\":";
    body += String((int)gpsBuffer[idx].speedKmh);
    body += "}";
    pointsAdded++;
  }

  // If no buffered points, use the current position
  if (pointsAdded == 0) {
    float heading = gps.course.isValid() ? gps.course.deg() : 0;
    float speedKmh = gps.speed.isValid() ? gps.speed.kmph() : 0;

    body += "{\"latitude\":";
    body += String(lat, 6);
    body += ",\"longitude\":";
    body += String(lon, 6);
    body += ",\"heading\":";
    body += String((int)heading);
    body += ",\"speed\":";
    body += String((int)speedKmh);
    body += "}";
    pointsAdded = 1;
  }

  body += "]}";

  // Build the URL
  String url = "https://api.tomtom.com/snap-to-roads/1/snap-to-roads";
  url += "?key=";
  url += TOMTOM_API_KEY;

  Serial.printf("[API] Snap-to-Roads: %d points\n", pointsAdded);

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(body);

  if (httpCode != 200) {
    Serial.printf("[API] Snap-to-Roads failed: HTTP %d\n", httpCode);
    if (httpCode == 429) {
      Serial.println("[API] Rate limited! Consider increasing API interval.");
    }
    http.end();
    return -1;
  }

  String response = http.getString();
  http.end();

  // Parse the response
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.printf("[API] JSON parse error: %s\n", err.c_str());
    return -1;
  }

  // Try to find speed limit in the response.
  // TomTom's response format may vary — we check multiple paths.
  int limit = -1;

  // Path 1: snappedPoints array with speedLimit
  JsonArray snapped = doc["snappedPoints"];
  if (snapped && snapped.size() > 0) {
    // Use the LAST snapped point (most recent position)
    JsonObject lastPoint = snapped[snapped.size() - 1];

    // Try speedLimit as a direct number
    if (lastPoint.containsKey("speedLimit")) {
      JsonVariant sl = lastPoint["speedLimit"];
      if (sl.is<int>()) {
        limit = sl.as<int>();
      } else if (sl.is<JsonObject>()) {
        // Could be {"value": 65, "unit": "MPH"}
        limit = sl["value"] | -1;
      } else if (sl.is<const char*>()) {
        String s = String(sl.as<const char*>());
        s.replace("MPH", ""); s.replace("mph", "");
        s.replace("KPH", ""); s.replace("kph", "");
        s.trim();
        limit = (int)lround(s.toFloat());
      }
    }

    // Path 2: nested under road info
    if (limit <= 0 && lastPoint.containsKey("road")) {
      JsonVariant roadSl = lastPoint["road"]["speedLimit"];
      if (!roadSl.isNull()) {
        if (roadSl.is<int>()) limit = roadSl.as<int>();
        else if (roadSl.is<JsonObject>()) limit = roadSl["value"] | -1;
      }
    }
  }

  // Sanity check — speed limits should be between 5 and 130 mph
  if (limit > 0 && limit <= 130) {
    Serial.printf("[API] Snap-to-Roads speed limit: %d mph\n", limit);
    return limit;
  }

  Serial.println("[API] Snap-to-Roads: no speed limit in response");
  return -1;
}


// --- Reverse Geocode (FALLBACK — less accurate but reliable) ---
// This is the original approach from Rev3. We keep it as a backup
// in case Snap-to-Roads doesn't work or doesn't return a speed limit.

int fetchSpeedLimitReverseGeocode(double lat, double lon) {
  String url = "https://api.tomtom.com/search/2/reverseGeocode/";
  url += String(lat, 6);
  url += ",";
  url += String(lon, 6);
  url += ".json?key=";
  url += TOMTOM_API_KEY;
  url += "&returnSpeedLimit=true";
  url += "&radius=";
  url += String(GEOCODE_RADIUS_M);   // Reduced from 100m to 25m for better accuracy

  Serial.println("[API] Fallback: Reverse Geocode");

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("[API] Reverse Geocode failed: HTTP %d\n", httpCode);
    http.end();
    return -1;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<JSON_BUFFER_SIZE> doc;
  if (deserializeJson(doc, payload)) return -1;

  JsonVariant sl = doc["addresses"][0]["address"]["speedLimit"];
  if (sl.isNull()) return -1;

  int limit = -1;
  if (sl.is<int>()) {
    limit = sl.as<int>();
  } else if (sl.is<const char*>()) {
    String s = String(sl.as<const char*>());
    s.replace("MPH", ""); s.replace("mph", "");
    s.trim();
    limit = (int)lround(s.toFloat());
  }

  if (limit > 0 && limit <= 130) {
    Serial.printf("[API] Reverse Geocode speed limit: %d mph\n", limit);
    return limit;
  }

  return -1;
}


// --- Main speed limit fetch — tries Snap-to-Roads first, falls back to Reverse Geocode ---
int fetchSpeedLimit(double lat, double lon) {
  // Must have WiFi
  if (WiFi.status() != WL_CONNECTED) return -1;

  // Drain any GPS data that arrived while we were doing HTTP
  // (prevents data loss during the API call)
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
    lastByteMs = millis();
  }

  // Try Snap-to-Roads first (better accuracy)
  int limit = fetchSpeedLimitSnapToRoads(lat, lon);

  // Drain GPS again between API calls
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
    lastByteMs = millis();
  }

  // If Snap-to-Roads didn't work, try Reverse Geocode
  if (limit <= 0) {
    limit = fetchSpeedLimitReverseGeocode(lat, lon);
  }

  // Drain GPS one more time after all API work is done
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
    lastByteMs = millis();
  }

  return limit;
}


// =============================================================
// SECTION 12: TOP STATUS BAR
// =============================================================
// The top 16 pixels of the screen show status icons:
//   [WiFi bars] [GPS icon] [Sat count] ............ [! API warning]

int wifiBars() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -60) return 3;   // Strong
  if (rssi >= -70) return 2;   // Medium
  if (rssi >= -80) return 1;   // Weak
  return 1;
}

void drawWifiIcon(int x, int y, int bars, uint16_t col) {
  display.fillRect(x, y, 18, 16, C_BLACK());
  int baseY = y + 14;
  int barW = 3;
  int gap = 2;

  for (int i = 0; i < 3; i++) {
    int h = 3 + i * 4;
    int bx = x + i * (barW + gap);
    uint16_t c = (i < bars) ? col : scaleColor(C_GRAY());
    display.fillRect(bx, baseY - h, barW, h, c);
  }
}

void drawGpsIcon(int x, int y, bool solid, uint16_t col) {
  display.fillRect(x, y, 16, 16, C_BLACK());
  int cx = x + 8;
  int cy = y + 6;

  if (solid) {
    display.fillCircle(cx, cy, 4, col);
    display.fillTriangle(cx, y + 14, cx - 3, y + 9, cx + 3, y + 9, col);
  } else {
    display.drawCircle(cx, cy, 4, scaleColor(C_GRAY()));
    display.drawTriangle(cx, y + 14, cx - 3, y + 9, cx + 3, y + 9, scaleColor(C_GRAY()));
  }
}

void drawSatNumber(int x, int y, int sats) {
  display.fillRect(x, y, 24, 16, C_BLACK());
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(scaleColor(C_WHITE()));
  display.setCursor(x, y + 13);
  if (sats < 0) sats = 0;
  display.print(sats);
}

void drawApiWarnIcon(int x, int y, bool warn) {
  display.fillRect(x, y, 12, 16, C_BLACK());
  if (warn) {
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(scaleColor(C_YELLOW()));
    display.setCursor(x + 3, y + 13);
    display.print("!");
  }
}

void drawTopStatusBar(bool gpsFix, bool blinkSolid, int sats, int wBars, bool apiWarn) {
  bool solid = gpsFix ? true : blinkSolid;

  // Skip redraw if nothing changed (SPI writes are slow)
  if (solid == lastGpsIconSolid &&
      sats == lastShownSats &&
      wBars == lastWifiBars &&
      apiWarn == lastApiWarn) {
    return;
  }

  display.fillRect(0, 0, 128, 16, C_BLACK());

  drawWifiIcon(2, 0, wBars, scaleColor(C_GREEN()));
  drawGpsIcon(26, 0, solid, gpsFix ? scaleColor(C_GREEN()) : scaleColor(C_YELLOW()));
  drawSatNumber(48, 0, sats);
  drawApiWarnIcon(112, 0, apiWarn);

  lastGpsIconSolid = solid;
  lastShownSats = sats;
  lastWifiBars = wBars;
  lastApiWarn = apiWarn;
}


// =============================================================
// SECTION 13: MAIN DISPLAY AREA
// =============================================================
// Shows the speed limit in big text, centered on screen.

void clearMain() {
  display.fillRect(0, 16, 128, 112, C_BLACK());
}

void drawMain(const String& bigText, uint16_t bigColor) {
  uint16_t scaledBig = scaleColor(bigColor);
  if (bigText == lastMainText && scaledBig == lastMainColor) return;

  clearMain();

  // Draw the big speed limit number
  display.setFont(&FreeSansBold24pt7b);
  display.setTextColor(scaledBig);

  // Calculate position to center the text
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(bigText, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_W - (int)w) / 2;
  int y = 80;
  display.setCursor(x, y);
  display.print(bigText);

  // Draw "MPH" label below
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(scaleColor(C_WHITE()));
  String mph = "MPH";
  display.getTextBounds(mph, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - (int)w) / 2, 114);
  display.print(mph);

  lastMainText = bigText;
  lastMainColor = scaledBig;
}


// =============================================================
// SECTION 14: MENU SYSTEM
// =============================================================
// Long-press the button to enter the menu.
// Short-press cycles through options, long-press changes the selected option.
// Very-long-press exits the menu.
//
// Menu items:
//   0 = Brightness          (cycles through 8 levels)
//   1 = Yellow alert at     (mph over limit to turn yellow)
//   2 = Red alert at        (mph over limit to turn red)
//   3 = API Interval        (seconds between API calls)
//   4 = API Move            (meters of movement before API call)
//   5 = GPS Reset           (warm restart of GPS module)
//   6 = Exit                (leave the menu)

void drawMenuScreen() {
  if (!menuDirty) return;
  menuDirty = false;

  clearMain();
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(scaleColor(C_WHITE()));

  display.setCursor(6, 32);
  display.print("SETTINGS");

  String label, value;
  switch (menuIndex) {
    case 0: label = "Brightness";   value = String(brightIdx + 1) + "/" + String(BRIGHT_LEVEL_COUNT); break;
    case 1: label = "Yellow at";    value = String(yellowOverMph) + "+ mph"; break;
    case 2: label = "Red at";       value = String(redOverMph) + "+ mph"; break;
    case 3: label = "API Interval"; value = String(apiMinIntervalMs / 1000) + " sec"; break;
    case 4: label = "API Move";     value = String((int)lround(apiMinMoveM)) + " m"; break;
    case 5: label = "GPS Reset";    value = "Warm restart"; break;
    default: label = "Exit";        value = "Hold to exit"; break;
  }

  display.setCursor(6, 60);
  display.print("> ");
  display.print(label);

  display.setTextColor(scaleColor(C_GRAY()));
  display.setCursor(6, 86);
  display.print(value);

  display.setTextColor(scaleColor(C_GRAY()));
  display.setCursor(6, 112);
  display.print("Short: Next");
  display.setCursor(6, 124);
  display.print("Hold: Select");
}

void menuNext() {
  menuIndex++;
  if (menuIndex > 6) menuIndex = 0;
  menuDirty = true;
}

void menuChangeCurrent() {
  switch (menuIndex) {
    case 0:
      nextBrightnessLevel();
      break;
    case 1:
      yellowOverMph++;
      if (yellowOverMph > 8) yellowOverMph = 1;
      if (redOverMph <= yellowOverMph) redOverMph = yellowOverMph + 1;
      saveSettings();
      menuDirty = true;
      break;
    case 2:
      redOverMph++;
      if (redOverMph > 12) redOverMph = max(2, yellowOverMph + 1);
      if (redOverMph <= yellowOverMph) redOverMph = yellowOverMph + 1;
      saveSettings();
      menuDirty = true;
      break;
    case 3: {
      const uint32_t opts[] = { 2000, 3000, 5000, 8000, 10000, 15000, 20000, 30000 };
      int numOpts = sizeof(opts) / sizeof(opts[0]);
      int idx = 0;
      for (int i = 0; i < numOpts; i++) {
        if (apiMinIntervalMs == opts[i]) { idx = i; break; }
      }
      idx = (idx + 1) % numOpts;
      apiMinIntervalMs = opts[idx];
      saveSettings();
      menuDirty = true;
    } break;
    case 4: {
      const uint32_t opts[] = { 10, 20, 30, 50, 75, 100, 150, 200, 300 };
      int numOpts = sizeof(opts) / sizeof(opts[0]);
      int cur = (int)lround(apiMinMoveM);
      int idx = 0;
      for (int i = 0; i < numOpts; i++) {
        if (cur == (int)opts[i]) { idx = i; break; }
      }
      idx = (idx + 1) % numOpts;
      apiMinMoveM = (float)opts[idx];
      saveSettings();
      menuDirty = true;
    } break;
    case 5:
      gpsWarmRestart();
      menuDirty = true;
      break;
    default:
      // Exit menu
      menuActive = false;
      menuDirty = true;
      lastMainText = "";
      lastMainColor = 0xFFFF;
      break;
  }
}

void enterMenu() {
  menuActive = true;
  menuIndex = 0;
  menuDirty = true;
  lastShownSats = -999;
  lastWifiBars = -1;
  lastApiWarn = !lastApiWarn;
}

void exitMenu() {
  menuActive = false;
  menuDirty = true;
  lastMainText = "";
  lastMainColor = 0xFFFF;
}


// =============================================================
// SECTION 15: SETUP (runs once on boot)
// =============================================================

void setup() {
  // Start USB serial for debug output (you can read this in Arduino Serial Monitor)
  Serial.begin(115200);
  delay(300);
  Serial.println("========================================");
  Serial.println("  Speed Limit Display — Rev 4");
  Serial.println("========================================");
  Serial.println("Commands: 'r' = reboot, 'd' = debug info");
  Serial.println();

  bootMs = millis();

  // Initialize button pin with internal pull-up resistor
  pinMode(BTN_PIN, INPUT_PULLUP);

  // Load saved settings from flash memory
  prefs.begin("slim", false);
  loadSettings();

  // Initialize the OLED display
  SPI.begin(SPI_SCLK, -1, SPI_MOSI);
  display.begin();
  display.fillScreen(C_BLACK());

  // Show startup message
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(C_WHITE());
  display.setCursor(10, 50);
  display.print("Speed");
  display.setCursor(10, 70);
  display.print("Limit v4");
  display.setTextColor(C_GRAY());
  display.setCursor(10, 100);
  display.print("Starting...");

  // Initialize GPS at default 9600 baud, then switch to 38400
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(100);
  gpsConfigureHighRate();

  // Initialize GPS point buffer
  for (int i = 0; i < GPS_BUFFER_SIZE; i++) {
    gpsBuffer[i].valid = false;
  }

  // Initialize speed limit history
  for (int i = 0; i < LIMIT_HISTORY_SIZE; i++) {
    limitHistory[i] = -1;
  }

  // Start WiFi (non-blocking — won't freeze here)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  wifiConnecting = true;
  wifiConnectStart = millis();
  Serial.printf("[WiFi] Connecting to: %s\n", WIFI_SSID);

  // Setup watchdog timer — reboots if main loop freezes
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);  // true = reboot on timeout
  esp_task_wdt_add(NULL);   // Monitor this task (the main loop task)
  Serial.printf("[WDT] Watchdog armed: %d second timeout\n", WATCHDOG_TIMEOUT_SEC);

  // Reset display cache
  lastMainText     = "";
  lastMainColor    = 0xFFFF;
  lastShownSats    = -999;
  lastWifiBars     = -1;
  lastGpsIconSolid = false;
  lastApiWarn      = false;

  // Brief pause to show startup screen, then switch to main display
  delay(1500);
  display.fillScreen(C_BLACK());
  drawMain("--", C_GRAY());

  Serial.println("[BOOT] Setup complete. Waiting for GPS fix...");
}


// =============================================================
// SECTION 16: MAIN LOOP (runs continuously)
// =============================================================
// This is the heartbeat of the device. It runs as fast as possible
// and handles everything: GPS reading, button input, display updates,
// WiFi management, and API calls.
//
// IMPORTANT: Nothing in here should "block" (freeze) for more than
// a few milliseconds, EXCEPT the API call (which we can't avoid on
// a single-core ESP32). We drain GPS data before and after API calls
// to minimize data loss.

void loop() {
  // Pet the watchdog — "I'm still alive!"
  // If we don't do this within WATCHDOG_TIMEOUT_SEC seconds, the ESP32 reboots.
  esp_task_wdt_reset();

  // Check for serial commands (type 'r' to reboot, 'd' for debug)
  checkSerialCommands();

  // --- Button handling ---
  BtnEvent ev = pollButtonEvent();
  if (!menuActive) {
    if (ev == BTN_SHORT) nextBrightnessLevel();
    else if (ev == BTN_LONG) enterMenu();
  } else {
    if (ev == BTN_SHORT) menuNext();
    else if (ev == BTN_LONG) menuChangeCurrent();
    else if (ev == BTN_VLONG) exitMenu();
  }

  // --- Read ALL available GPS data ---
  // We read everything available right now. The GPS module sends data
  // constantly (10 times per second at 10Hz), so we need to keep up.
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
    lastByteMs = millis();
  }

  // --- GPS fix tracking ---
  bool nmeaFlowing = (millis() - lastByteMs) < 1500;
  bool fixValid = gps.location.isValid() && gps.location.age() < 2000;

  if (fixValid) {
    haveFixEver = true;
    lastFixMs = millis();
    if (firstFixMs == 0) firstFixMs = millis();

    // Buffer GPS point every 500ms (2 points per second)
    // More frequent than API calls, so we build up a trajectory
    if ((millis() - lastBufferMs) >= 500) {
      bufferGpsPoint();
    }
  }

  bool inGrace       = (millis() - bootMs) < GPS_GRACE_MS;
  bool recentlyHadFix = (millis() - lastFixMs) < GPS_LOST_HOLD_MS;
  bool acquiring      = nmeaFlowing && !fixValid && (inGrace || !haveFixEver);
  bool blinkOn        = ((millis() / GPS_BLINK_MS) % 2) == 0;

  int sats = gps.satellites.isValid() ? gps.satellites.value() : 0;

  // --- WiFi management (non-blocking) ---
  bool wifiReady = wifiManage();
  int wBars = wifiReady ? wifiBars() : 0;

  // --- API warning icon ---
  bool recentFail = (millis() - lastApiFailMs) < API_WARN_WINDOW_MS;
  bool noLimitTooLong = (fixValid && currentSpeedLimitMph <= 0 &&
                         firstFixMs != 0 && (millis() - firstFixMs) > API_NO_LIMIT_WARN_MS);
  bool apiWarn = recentFail || noLimitTooLong;

  // --- Draw status bar ---
  drawTopStatusBar(fixValid, acquiring ? blinkOn : false, sats, wBars, apiWarn);

  // --- Menu mode ---
  if (menuActive) {
    drawMenuScreen();
    delay(25);
    return;   // Skip everything else while in the menu
  }

  // --- No GPS data flowing? ---
  if (!nmeaFlowing) { delay(25); return; }
  if (!fixValid && !haveFixEver) { drawMain("--", C_GRAY()); delay(25); return; }
  if (!fixValid && !recentlyHadFix) { drawMain("--", C_GRAY()); delay(25); return; }

  // --- Get current position ---
  double lat = gps.location.isValid() ? gps.location.lat() : lastApiLat;
  double lon = gps.location.isValid() ? gps.location.lng() : lastApiLon;

  // --- Get current speed (with dead zone) ---
  int speedMph = 0;
  if (gps.speed.isValid() && gps.speed.age() < 2000) {
    speedMph = (int)lround(gps.speed.mph());

    // Dead zone: GPS is noisy at low speeds. If the GPS says 1-2 mph
    // when you're actually parked, we treat it as 0 to prevent false alerts.
    if (speedMph < SPEED_DEAD_ZONE_MPH) speedMph = 0;
  }

  // --- Should we call the API? ---
  bool timeOk = (millis() - lastApiMs) >= apiMinIntervalMs;
  bool moveOk = true;
  if (haveApiPos) {
    moveOk = distanceMeters(lat, lon, lastApiLat, lastApiLon) >= apiMinMoveM;
  }

  if (timeOk && moveOk && wifiReady) {
    // Make the API call
    int rawLimit = fetchSpeedLimit(lat, lon);

    if (rawLimit > 0) {
      // Apply smoothing — rejects wild jumps from false road matches
      int smoothed = smoothSpeedLimit(rawLimit);
      currentSpeedLimitMph = smoothed;
      lastGoodLimitMs = millis();
      lastApiLat = lat;
      lastApiLon = lon;
      haveApiPos = true;
    } else {
      lastApiFailMs = millis();
    }

    lastApiMs = millis();
  }

  // --- Is the speed limit still fresh enough to display? ---
  bool haveRecentLimit = (currentSpeedLimitMph > 0) &&
                         ((millis() - lastGoodLimitMs) < LIMIT_HOLD_MS);

  if (!haveRecentLimit) {
    drawMain("--", C_GRAY());
    delay(25);
    return;
  }

  // --- Update overspeed state and display ---
  overState = computeOverState(speedMph, currentSpeedLimitMph);
  drawMain(String(currentSpeedLimitMph), colorForOverState(overState));

  delay(25);
}
