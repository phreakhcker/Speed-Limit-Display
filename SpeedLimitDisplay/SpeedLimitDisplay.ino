// =============================================
// SPEED LIMIT DISPLAY — Rev 4.1 (Phase 2)
// =============================================
// ESP32 Mini + GPS + SSD1351 OLED + TomTom API
//
// What this does:
//   - Reads your GPS position and heading
//   - Asks TomTom "what road am I on and what's the speed limit?"
//   - Shows the speed limit on the OLED display
//   - Changes color: GREEN (ok), YELLOW (close to limit), RED (over limit)
//
// Rev4 Phase 1 changes (vs Rev3):
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
// Rev4.1 Phase 2 changes:
//   - Dual-core background HTTP: API calls run on Core 0 while GPS/display
//     runs on Core 1. Display never freezes. Falls back to inline on single-core.
//   - API rate limit handling: detects HTTP 429, exponential backoff
//   - Daily API quota tracking with warnings at 80% usage
//   - WiFi exponential backoff: smarter reconnection (5s→10s→20s→40s→60s max)
//   - WiFi disconnect/reconnect event logging
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
#include "captive_portal.h"      // WiFi setup portal for first boot

// --- Runtime credentials (loaded from NVS or config_secrets.h) ---
static String   runtimeSSID[2];
static String   runtimePASS[2];
static String   runtimeAPIKey;
static int      runtimeNetCount = 0;
static bool     usePortalCreds  = false;   // true = loaded from NVS, false = from config_secrets.h


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
int      lastShownSpeed    = -999;    // Last displayed current speed (for redraw check)
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

// --- WiFi state (non-blocking with exponential backoff) ---
bool     wifiConnecting    = false;   // Are we in the middle of connecting?
uint32_t wifiConnectStart  = 0;       // When we started the current connection attempt
uint32_t wifiLastAttemptMs = 0;       // When we last tried to connect
uint32_t wifiRetryMs       = WIFI_RETRY_INITIAL_MS;  // Current backoff delay (grows on failures)
int      wifiFailCount     = 0;       // Consecutive failed attempts
bool     wifiWasConnected  = false;   // Were we connected last loop? (for disconnect logging)

// --- API rate limiting ---
// Tracks usage and implements exponential backoff when rate-limited.
int      apiCallsToday     = 0;       // Approximate API calls made today (resets on reboot)
uint32_t apiBackoffUntil   = 0;       // Don't make API calls until this millis() timestamp
uint32_t apiBackoffMs      = RATE_LIMIT_INITIAL_MS;  // Current backoff delay
bool     apiRateLimited    = false;   // Are we currently in a rate-limit backoff?
bool     apiQuotaWarned    = false;   // Have we already warned about approaching quota?

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

// --- Dual-core background HTTP ---
// On dual-core ESP32s, API calls run on Core 0 (background) while the main
// loop runs on Core 1. This prevents HTTP requests from freezing the display
// and losing GPS data.
//
// How it works:
//   1. Main loop sets requestNeeded = true and fills in the request lat/lon
//   2. Background task sees the flag, makes the HTTP call, stores the result
//   3. Main loop sees resultReady = true and reads the speed limit
//
// "volatile" tells the compiler: "another thread might change this variable
// at any time, so always read it from memory, don't cache it in a register."
struct HttpRequest {
  volatile bool    requestNeeded;   // Main loop → background: "please make an API call"
  volatile bool    resultReady;     // Background → main loop: "result is ready"
  volatile bool    taskRunning;     // Is the background task alive?
  double           lat;             // Request: GPS latitude
  double           lon;             // Request: GPS longitude
  GpsPoint         points[GPS_BUFFER_SIZE];  // Copy of GPS buffer for the request
  int              pointCount;      // How many valid points in the buffer
  volatile int     resultLimit;     // Response: speed limit (-1 = failed)
  volatile bool    resultFailed;    // Response: did the API call fail?
  volatile int     httpCode;        // Response: HTTP status code (for rate limit detection)
};

HttpRequest httpReq = { false, false, false, 0, 0, {}, 0, -1, false, 0 };
bool dualCoreAvailable = false;     // Set in setup() based on chip type

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
      Serial.printf("  Dual-core: %s\n", dualCoreAvailable ? "YES (HTTP on Core 0)" : "NO (inline HTTP)");
      Serial.printf("  API calls today: ~%d / %d\n", apiCallsToday, API_DAILY_LIMIT);
      Serial.printf("  Rate limited: %s", apiRateLimited ? "YES" : "no");
      if (apiRateLimited) Serial.printf(" (backoff: %lu ms)", apiBackoffMs);
      Serial.println();
      Serial.printf("  WiFi fails: %d (retry delay: %lu ms)\n", wifiFailCount, wifiRetryMs);
      Serial.println("==================");
    }
    else if (c == 'f' || c == 'F') {
      Serial.println("[CMD] Factory reset — clearing all credentials...");
      portalClearCreds();
      delay(100);
      Serial.println("[CMD] Rebooting into setup mode...");
      ESP.restart();
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
// SECTION 7: GPS FUNCTIONS (u-blox UBX Protocol)
// =============================================================
// The GT-U7 GPS module uses a u-blox NEO-6M compatible chip.
// u-blox modules speak the UBX BINARY protocol — NOT the PMTK
// text commands used by MediaTek GPS chips.
//
// UBX message format:
//   [0xB5] [0x62] [class] [id] [length-lo] [length-hi] [payload...] [ckA] [ckB]
//
// The checksum (ckA, ckB) is a Fletcher checksum over class+id+length+payload.
// We calculate it automatically in the ubxSend() function below.

// Send a UBX binary message to the GPS module.
// This handles the framing (sync bytes), checksum calculation, and transmission.
// You just provide the message class, ID, and payload — this function does the rest.
void ubxSend(uint8_t msgClass, uint8_t msgId, const uint8_t* payload, uint16_t len) {
  // Sync characters — every UBX message starts with these two bytes
  GPSSerial.write(0xB5);
  GPSSerial.write(0x62);

  // Fletcher checksum accumulators
  // "Fletcher checksum" = a simple but effective error-detection algorithm.
  // It runs over class + id + length + payload bytes.
  uint8_t ckA = 0, ckB = 0;

  // Helper: add a byte to the checksum AND send it
  // We use a local variable approach since lambdas can be tricky on some Arduino cores.
  // Instead, we'll inline it.

  // Class byte
  GPSSerial.write(msgClass);
  ckA += msgClass; ckB += ckA;

  // ID byte
  GPSSerial.write(msgId);
  ckA += msgId; ckB += ckA;

  // Length (2 bytes, little-endian — low byte first)
  uint8_t lenLo = (uint8_t)(len & 0xFF);
  uint8_t lenHi = (uint8_t)(len >> 8);
  GPSSerial.write(lenLo);
  ckA += lenLo; ckB += ckA;
  GPSSerial.write(lenHi);
  ckA += lenHi; ckB += ckA;

  // Payload bytes
  for (uint16_t i = 0; i < len; i++) {
    GPSSerial.write(payload[i]);
    ckA += payload[i]; ckB += ckA;
  }

  // Checksum
  GPSSerial.write(ckA);
  GPSSerial.write(ckB);
}

// Set GPS update rate.
// The NEO-6M can reliably do 5Hz (200ms between fixes).
// Some clones claim 10Hz but it's often unstable — 5Hz is the safe max.
// Even 5Hz is 5x better than the 1Hz default!
void gpsSetUpdateRate() {
  // UBX-CFG-RATE (class=0x06, id=0x08)
  // Payload (6 bytes):
  //   measRate: measurement interval in ms (200 = 5Hz)
  //   navRate:  how many measurements per navigation solution (1 = every measurement)
  //   timeRef:  time reference (1 = GPS time)
  uint8_t payload[] = {
    0xC8, 0x00,   // measRate = 200ms (5Hz) — little-endian: 0x00C8 = 200
    0x01, 0x00,   // navRate = 1
    0x01, 0x00    // timeRef = GPS time
  };
  ubxSend(0x06, 0x08, payload, sizeof(payload));
  Serial.println("[GPS] Update rate set to 5Hz (200ms)");
}

// Set GPS to automotive navigation mode.
// This tells the GPS chip "I'm in a car" so it can optimize its filtering:
//   - Expects higher speeds and smoother turns
//   - Rejects impossible position jumps
//   - Better accuracy on roads vs open field
void gpsSetAutomotiveMode() {
  // UBX-CFG-NAV5 (class=0x06, id=0x24)
  // Payload is 36 bytes. Most are zero (don't change).
  // We only set the mask (which fields to apply) and dynModel.
  uint8_t payload[36] = {0};
  payload[0] = 0x01;   // mask low byte: apply dynModel only
  payload[1] = 0x00;   // mask high byte
  payload[2] = 0x04;   // dynModel = 4 (Automotive)
  // All other bytes stay 0 (don't change those settings)

  // dynModel options for reference:
  //   0 = Portable (default)
  //   2 = Stationary
  //   3 = Pedestrian
  //   4 = Automotive  ← what we want
  //   5 = Sea
  //   6 = Airborne <1G
  //   7 = Airborne <2G
  //   8 = Airborne <4G

  ubxSend(0x06, 0x24, payload, sizeof(payload));
  Serial.println("[GPS] Navigation mode set to Automotive");
}

// Configure which NMEA messages the GPS sends.
// We only want RMC (position, speed, heading) and GGA (fix quality, satellites).
// Disabling the others (GLL, GSA, GSV, VTG) reduces serial traffic,
// which is important at higher baud rates and update rates.
void gpsConfigureMessages() {
  // UBX-CFG-MSG (class=0x06, id=0x01)
  // Short form: 3-byte payload = [msgClass, msgId, rate]
  // rate=0 means OFF, rate=1 means send every nav solution

  // NMEA message class is 0xF0
  // Message IDs:
  //   0x00 = GGA (fix data, satellites)     — KEEP ON
  //   0x01 = GLL (lat/lon)                  — turn off
  //   0x02 = GSA (DOP and active satellites) — turn off
  //   0x03 = GSV (satellites in view)        — turn off
  //   0x04 = RMC (position, speed, heading)  — KEEP ON
  //   0x05 = VTG (track and ground speed)    — turn off

  struct MsgConfig { uint8_t id; uint8_t rate; const char* name; };
  MsgConfig msgs[] = {
    { 0x00, 1, "GGA" },   // ON  — has satellite count and fix quality
    { 0x01, 0, "GLL" },   // OFF — redundant with RMC
    { 0x02, 0, "GSA" },   // OFF — DOP info we don't use
    { 0x03, 0, "GSV" },   // OFF — satellite detail we don't need
    { 0x04, 1, "RMC" },   // ON  — has position, speed, heading (the important one)
    { 0x05, 0, "VTG" },   // OFF — redundant with RMC
  };

  for (int i = 0; i < 6; i++) {
    uint8_t payload[] = { 0xF0, msgs[i].id, msgs[i].rate };
    ubxSend(0x06, 0x01, payload, sizeof(payload));
    delay(50);   // Give GPS time to process each command
    Serial.printf("[GPS] %s: %s\n", msgs[i].name, msgs[i].rate ? "ON" : "off");
  }
}

// Change the GPS module's baud rate.
// After sending this command, the GPS immediately switches to the new baud rate.
// We then have to switch our serial port to match.
void gpsSetBaudRate(uint32_t baud) {
  // UBX-CFG-PRT (class=0x06, id=0x00) — configure UART port
  // Payload is 20 bytes for UART configuration.
  uint8_t payload[20] = {0};

  payload[0] = 0x01;   // Port ID = 1 (UART1, the main serial port)
  // bytes 1-3: reserved (leave 0)

  // bytes 4-7: UART mode = 8N1 (8 data bits, no parity, 1 stop bit)
  // The magic value for 8N1 is 0x000008D0
  payload[4] = 0xD0;
  payload[5] = 0x08;
  payload[6] = 0x00;
  payload[7] = 0x00;

  // bytes 8-11: baud rate (little-endian)
  // 38400 = 0x00009600
  payload[8]  = (uint8_t)(baud & 0xFF);
  payload[9]  = (uint8_t)((baud >> 8) & 0xFF);
  payload[10] = (uint8_t)((baud >> 16) & 0xFF);
  payload[11] = (uint8_t)((baud >> 24) & 0xFF);

  // bytes 12-13: input protocol mask (accept both UBX and NMEA)
  payload[12] = 0x03;   // UBX + NMEA
  payload[13] = 0x00;

  // bytes 14-15: output protocol mask (send both UBX and NMEA)
  payload[14] = 0x03;   // UBX + NMEA
  payload[15] = 0x00;

  // bytes 16-19: flags (leave 0)

  ubxSend(0x06, 0x00, payload, sizeof(payload));
  Serial.printf("[GPS] Baud rate command sent: %lu\n", baud);
}

// Save current GPS configuration to the module's flash memory.
// Without this, the settings reset every time the GPS loses power.
// Not all u-blox clones support this — if it doesn't work, the
// gpsConfigureHighRate() function re-applies settings on every boot anyway.
void gpsSaveConfig() {
  // UBX-CFG-CFG (class=0x06, id=0x09)
  // Payload: clearMask(4) + saveMask(4) + loadMask(4) + deviceMask(1) = 13 bytes
  uint8_t payload[13] = {0};
  // clearMask = 0 (don't clear anything)
  // saveMask = save all sections
  payload[4] = 0xFF; payload[5] = 0xFF; payload[6] = 0x00; payload[7] = 0x00;
  // loadMask = 0 (don't load)
  // deviceMask = 0x07 (BBR + Flash + EEPROM)
  payload[12] = 0x07;

  ubxSend(0x06, 0x09, payload, sizeof(payload));
  Serial.println("[GPS] Configuration saved to flash");
}

// Master GPS configuration function.
// Runs at boot to set up the GPS for our use case.
// Sends all commands at the default 9600 baud, then switches to 38400.
void gpsConfigureHighRate() {
  Serial.println("[GPS] Configuring u-blox GPS module...");

  // Step 1: Configure messages (at default 9600 baud)
  gpsConfigureMessages();
  delay(100);

  // Step 2: Set automotive navigation mode
  gpsSetAutomotiveMode();
  delay(100);

  // Step 3: Set 5Hz update rate
  gpsSetUpdateRate();
  delay(100);

  // Step 4: Change baud rate to 38400
  // IMPORTANT: After this command, the GPS immediately switches.
  // Our serial port is still at 9600, so we need to switch too.
  gpsSetBaudRate(GPS_TARGET_BAUD);
  delay(100);   // Give GPS time to switch

  // Step 5: Switch our serial port to match the new baud rate
  GPSSerial.end();
  delay(50);
  GPSSerial.begin(GPS_TARGET_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(200);

  // Step 6: Try to save config to GPS flash (may not work on all clones)
  gpsSaveConfig();
  delay(100);

  Serial.printf("[GPS] Configured: 5Hz, RMC+GGA, automotive mode, %lu baud\n", GPS_TARGET_BAUD);
}

// Warm restart — GPS keeps its almanac/ephemeris data but re-acquires satellites.
// Faster than a cold restart (~1 second vs ~27 seconds). Useful if GPS seems
// stuck or inaccurate — like clearing a brief brain freeze.
void gpsWarmRestart() {
  // UBX-CFG-RST (class=0x06, id=0x04)
  // Payload (4 bytes):
  //   navBbrMask: which data to clear (0x0001 = warm start — keep most data)
  //   resetMode:  how to reset (0x02 = controlled software reset, GPS only)
  //   reserved: 0
  uint8_t payload[] = {
    0x01, 0x00,   // navBbrMask = warm start
    0x02,         // resetMode = controlled software reset
    0x00          // reserved
  };
  ubxSend(0x06, 0x04, payload, sizeof(payload));
  Serial.println("[GPS] Warm restart sent (UBX-CFG-RST)");
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
// SECTION 8: WIFI (Non-Blocking with Exponential Backoff)
// =============================================================
// "Non-blocking" means we don't freeze the device waiting for WiFi.
// Instead, we start the connection and check back each loop iteration.
// Meanwhile, GPS keeps running, the display keeps updating, etc.
//
// "Exponential backoff" means: if WiFi fails, we wait longer between
// each retry. First retry after 5s, then 10s, then 20s, then 40s, etc.
// This prevents hammering the WiFi chip when you're out of range.
// Once we connect, the backoff resets to the short initial delay.

// Call this every loop iteration. It manages WiFi without blocking.
// Returns true if WiFi is currently connected and ready to use.
bool wifiManage() {
  bool connected = (WiFi.status() == WL_CONNECTED);

  // --- Detect disconnect events (for logging) ---
  if (wifiWasConnected && !connected) {
    Serial.println("[WiFi] CONNECTION LOST — will auto-reconnect");
    wifiWasConnected = false;
    wifiConnecting = false;
    // Don't reset backoff here — let it grow if we keep losing connection
  }

  // --- Already connected? Reset backoff and return. ---
  if (connected) {
    if (!wifiWasConnected) {
      // Just connected! Log it and reset backoff.
      Serial.printf("[WiFi] Connected! IP: %s  RSSI: %d dBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      wifiFailCount = 0;
      wifiRetryMs = WIFI_RETRY_INITIAL_MS;   // Reset backoff for next time
    }
    wifiWasConnected = true;
    wifiConnecting = false;
    return true;
  }

  // --- Currently trying to connect? ---
  if (wifiConnecting) {
    if ((millis() - wifiConnectStart) > WIFI_CONNECT_TIMEOUT_MS) {
      // Timed out — give up this attempt
      wifiFailCount++;
      WiFi.disconnect();
      wifiConnecting = false;
      wifiLastAttemptMs = millis();

      // Exponential backoff: double the retry delay (capped at max)
      // "<<" is a bit shift — equivalent to multiplying by 2.
      // We cap the exponent at 6 to prevent overflow (2^6 = 64x max multiplier).
      int exponent = min(wifiFailCount, 6);
      wifiRetryMs = min(WIFI_RETRY_INITIAL_MS << exponent, WIFI_RETRY_MAX_MS);

      Serial.printf("[WiFi] Attempt #%d timed out. Next retry in %lu ms\n",
                    wifiFailCount, wifiRetryMs);
    }
    return false;   // Still waiting
  }

  // --- Not connected, not trying — should we start a new attempt? ---
  if ((millis() - wifiLastAttemptMs) >= wifiRetryMs) {
    // Scan for known networks and connect to the first one found.
    // This lets you have home WiFi + phone hotspot in config_secrets.h
    // and the device automatically picks whichever is available.
    Serial.printf("[WiFi] Scanning for known networks (attempt #%d)...\n",
                  wifiFailCount + 1);
    WiFi.mode(WIFI_STA);

    int n = WiFi.scanNetworks();
    bool found = false;

    for (int i = 0; i < n && !found; i++) {
      String scannedSSID = WiFi.SSID(i);
      for (int j = 0; j < runtimeNetCount; j++) {
        if (scannedSSID == runtimeSSID[j]) {
          Serial.printf("[WiFi] Found: %s (RSSI: %d) — connecting...\n",
                        runtimeSSID[j].c_str(), WiFi.RSSI(i));
          WiFi.begin(runtimeSSID[j].c_str(), runtimePASS[j].c_str());
          found = true;
          break;
        }
      }
    }

    WiFi.scanDelete();   // Free scan results memory

    if (!found) {
      // No known network found — try the first one in the list as a fallback
      // (it might be hidden or the scan might have missed it)
      Serial.printf("[WiFi] No known networks found. Trying: %s\n",
                    runtimeSSID[0].c_str());
      WiFi.begin(runtimeSSID[0].c_str(), runtimePASS[0].c_str());
    }

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

// --- Last HTTP status code from API calls ---
// Used by fetchSpeedLimit() to detect 429 rate limits.
// Set by both SnapToRoads and ReverseGeocode sub-functions.
int lastHttpCode = 0;

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
  url += runtimeAPIKey;

  Serial.printf("[API] Snap-to-Roads: %d points\n", pointsAdded);

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(body);
  lastHttpCode = httpCode;    // Store for rate limit detection

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
  url += runtimeAPIKey;
  url += "&returnSpeedLimit=true";
  url += "&radius=";
  url += String(GEOCODE_RADIUS_M);   // Reduced from 100m to 25m for better accuracy

  Serial.println("[API] Fallback: Reverse Geocode");

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);

  int httpCode = http.GET();
  lastHttpCode = httpCode;    // Store for rate limit detection
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
// Also handles rate limiting, quota tracking, and GPS data draining.
// The `outHttpCode` pointer lets the caller know what HTTP status we got
// (important for detecting 429 rate limits).
int fetchSpeedLimit(double lat, double lon, int* outHttpCode) {
  *outHttpCode = 0;
  lastHttpCode = 0;

  // Must have WiFi
  if (WiFi.status() != WL_CONNECTED) return -1;

  // Check if we're in a rate-limit backoff period
  if (apiRateLimited && millis() < apiBackoffUntil) {
    Serial.printf("[API] Rate-limited, waiting %lu more ms\n",
                  apiBackoffUntil - millis());
    return -1;
  }
  apiRateLimited = false;

  // Drain any GPS data that arrived while we were doing HTTP
  // (prevents data loss during the API call)
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
    lastByteMs = millis();
  }

  // Try Snap-to-Roads first (better accuracy)
  int limit = fetchSpeedLimitSnapToRoads(lat, lon);
  apiCallsToday++;   // Track usage

  // Drain GPS again between API calls
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
    lastByteMs = millis();
  }

  // If Snap-to-Roads didn't work, try Reverse Geocode
  if (limit <= 0) {
    limit = fetchSpeedLimitReverseGeocode(lat, lon);
    apiCallsToday++;   // This counts as a second API call
  }

  // Drain GPS one more time after all API work is done
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
    lastByteMs = millis();
  }

  // Propagate the HTTP status code to the caller
  *outHttpCode = lastHttpCode;

  // Check quota usage and warn if approaching limit
  int warnThreshold = (API_DAILY_LIMIT * API_WARN_PERCENT) / 100;
  if (apiCallsToday >= warnThreshold && !apiQuotaWarned) {
    Serial.printf("[API] WARNING: %d calls today (~%d%% of %d daily limit)\n",
                  apiCallsToday, (apiCallsToday * 100) / API_DAILY_LIMIT, API_DAILY_LIMIT);
    apiQuotaWarned = true;
  }

  return limit;
}

// Handle a rate-limit response (HTTP 429).
// Implements exponential backoff: wait longer each time we get rate-limited.
void handleRateLimit() {
  apiRateLimited = true;
  apiBackoffUntil = millis() + apiBackoffMs;
  Serial.printf("[API] RATE LIMITED (429)! Backing off for %lu ms\n", apiBackoffMs);

  // Double the backoff for next time (capped at max)
  apiBackoffMs = min(apiBackoffMs * 2, RATE_LIMIT_MAX_MS);
}

// Reset rate-limit backoff after a successful API call.
void resetRateLimitBackoff() {
  if (apiBackoffMs != RATE_LIMIT_INITIAL_MS) {
    apiBackoffMs = RATE_LIMIT_INITIAL_MS;
    Serial.println("[API] Rate limit backoff reset");
  }
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
// Shows two numbers:
//   - Your CURRENT SPEED (smaller, top area) — so you know how fast you're going
//   - The SPEED LIMIT (big, center) — color-coded green/yellow/red
//
// Layout on the 128x128 screen:
//   ┌────────────────┐
//   │ [status bar]   │  0-16   (16px)
//   │   45 mph       │  17-44  (current speed, small white text)
//   │     65         │  45-105 (speed limit, BIG colored text)
//   │    LIMIT       │  106-128 (label)
//   └────────────────┘

void clearMain() {
  display.fillRect(0, 16, 128, 112, C_BLACK());
}

// Draw just the current speed area (top portion) without redrawing everything.
// This updates frequently (GPS speed changes often) without flickering the
// speed limit number which changes rarely.
void drawCurrentSpeed(int speedMph) {
  if (speedMph == lastShownSpeed) return;   // No change, skip redraw

  // Clear only the current speed area (y=17 to y=48)
  display.fillRect(0, 17, 128, 31, C_BLACK());

  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(scaleColor(C_WHITE()));

  // Build the speed string (e.g., "45 mph")
  String speedStr;
  if (speedMph <= 0) {
    speedStr = "-- mph";
  } else {
    speedStr = String(speedMph) + " mph";
  }

  // Center it
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(speedStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - (int)w) / 2, 38);
  display.print(speedStr);

  lastShownSpeed = speedMph;
}

// Draw the speed limit (big number) and "LIMIT" label.
// Only redraws when the limit or color changes.
void drawMain(const String& bigText, uint16_t bigColor) {
  uint16_t scaledBig = scaleColor(bigColor);
  if (bigText == lastMainText && scaledBig == lastMainColor) return;

  // Clear only the speed limit area (y=48 to y=128)
  display.fillRect(0, 48, 128, 80, C_BLACK());

  // Draw the big speed limit number
  display.setFont(&FreeSansBold24pt7b);
  display.setTextColor(scaledBig);

  // Calculate position to center the text
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(bigText, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_W - (int)w) / 2;
  int y = 95;
  display.setCursor(x, y);
  display.print(bigText);

  // Draw "LIMIT" label below
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(scaleColor(C_GRAY()));
  String lbl = "LIMIT";
  display.getTextBounds(lbl, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - (int)w) / 2, 120);
  display.print(lbl);

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
  lastShownSpeed = -999;
}


// =============================================================
// SECTION 15: BACKGROUND HTTP TASK (Dual-Core)
// =============================================================
// The ESP32 (original and S3) has TWO CPU cores:
//   - Core 1: runs setup() and loop() — handles GPS, display, buttons
//   - Core 0: normally idle (used by WiFi/BT stack) — we put our HTTP calls here
//
// Why? HTTP requests take 1-5 seconds. During that time, on a single core,
// NOTHING else runs — GPS data piles up and overflows, the display freezes.
// By running HTTP on Core 0, the main loop on Core 1 keeps running smoothly.
//
// On single-core chips (ESP32-S2, ESP32-C3), this task doesn't start.
// Instead, API calls happen inline in the main loop (like Rev4 Phase 1).
//
// Communication between cores uses the `httpReq` struct with volatile flags.
// "volatile" = "always read from actual memory, not a cached copy."

// This function runs forever on Core 0. It sleeps most of the time,
// wakes up when the main loop sets requestNeeded = true, does the HTTP call,
// and stores the result.
void httpBackgroundTask(void* parameter) {
  Serial.println("[HTTP-TASK] Background HTTP task started on Core 0");
  httpReq.taskRunning = true;

  for (;;) {   // Run forever (FreeRTOS tasks must never return)
    // Wait for a request from the main loop
    if (!httpReq.requestNeeded) {
      vTaskDelay(pdMS_TO_TICKS(HTTP_TASK_POLL_MS));   // Sleep to save power
      continue;
    }

    // We have a request! Make the API call.
    int httpCode = 0;
    int limit = fetchSpeedLimit(httpReq.lat, httpReq.lon, &httpCode);

    // Store the result for the main loop to pick up
    httpReq.resultLimit  = limit;
    httpReq.resultFailed = (limit <= 0);
    httpReq.httpCode     = httpCode;
    httpReq.requestNeeded = false;   // Clear the request flag
    httpReq.resultReady   = true;    // Signal that result is available

    // Small delay to prevent busy-looping right after a result
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// Copy the current GPS buffer into the HTTP request struct.
// This gives the background task its own copy of the GPS data,
// so it doesn't conflict with the main loop writing new points.
void prepareHttpRequest(double lat, double lon) {
  httpReq.lat = lat;
  httpReq.lon = lon;

  // Copy GPS buffer
  httpReq.pointCount = 0;
  for (int i = 0; i < GPS_BUFFER_SIZE; i++) {
    httpReq.points[i] = gpsBuffer[i];   // Struct copy
    if (gpsBuffer[i].valid) httpReq.pointCount++;
  }

  httpReq.resultReady = false;
  httpReq.resultFailed = false;
  httpReq.requestNeeded = true;   // Signal the background task
}


// =============================================================
// SECTION 16: SETUP (runs once on boot)
// =============================================================

void setup() {
  // Start USB serial for debug output (you can read this in Arduino Serial Monitor)
  Serial.begin(115200);
  delay(300);
  Serial.println("========================================");
  Serial.println("  Speed Limit Display — Rev 4.1");
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
  display.print("Limit v4.1");
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

  // --- Check for saved credentials (captive portal) ---
  // If customer has set up via portal, use those credentials.
  // Otherwise fall back to hardcoded config_secrets.h (for development).
  if (portalHasCredentials()) {
    PortalCreds creds = portalLoadCreds();
    runtimeSSID[0] = creds.ssid;
    runtimePASS[0] = creds.pass;
    runtimeSSID[1] = creds.ssid2;
    runtimePASS[1] = creds.pass2;
    runtimeAPIKey  = creds.apikey;
    runtimeNetCount = creds.networkCount;
    usePortalCreds = true;
    Serial.printf("[WiFi] Loaded %d network(s) from portal setup\n", runtimeNetCount);
  } else {
    // Check if config_secrets.h has real credentials (not placeholders)
    bool hasHardcoded = (WIFI_NETWORK_COUNT > 0 &&
                         String(WIFI_NETWORKS[0][0]) != "Your_Home_WiFi" &&
                         String(TOMTOM_API_KEY) != "Your_TomTom_API_Key_Here");

    if (!hasHardcoded) {
      // No credentials anywhere — launch captive portal
      Serial.println("[Portal] No credentials found. Starting setup portal...");
      portalStart(display);

      // Stay in portal mode — don't continue to normal setup
      // The main loop will call portalLoop() instead of normal operation
      return;
    }

    // Use hardcoded credentials from config_secrets.h (developer mode)
    for (int i = 0; i < WIFI_NETWORK_COUNT && i < 2; i++) {
      runtimeSSID[i] = WIFI_NETWORKS[i][0];
      runtimePASS[i] = WIFI_NETWORKS[i][1];
    }
    runtimeAPIKey = TOMTOM_API_KEY;
    runtimeNetCount = min(WIFI_NETWORK_COUNT, 2);
    usePortalCreds = false;
    Serial.printf("[WiFi] Using hardcoded credentials (%d networks)\n", runtimeNetCount);
  }

  // Start WiFi (non-blocking — won't freeze here)
  WiFi.mode(WIFI_STA);
  WiFi.begin(runtimeSSID[0].c_str(), runtimePASS[0].c_str());
  wifiConnecting = true;
  wifiConnectStart = millis();
  Serial.printf("[WiFi] %d networks configured. Trying: %s\n",
                runtimeNetCount, runtimeSSID[0].c_str());

  // Setup watchdog timer — reboots if main loop freezes
  // The ESP32-C3 uses a newer API that takes a config struct instead of two arguments.
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = (uint32_t)WATCHDOG_TIMEOUT_SEC * 1000,  // Convert seconds to milliseconds
    .idle_core_mask = 0,       // Don't monitor idle tasks
    .trigger_panic = true      // Reboot on timeout
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);   // Monitor this task (the main loop task)
  Serial.printf("[WDT] Watchdog armed: %d second timeout\n", WATCHDOG_TIMEOUT_SEC);

  // --- Detect dual-core and launch background HTTP task ---
  // ESP32 (original, S3) = 2 cores. ESP32-S2, C3 = 1 core.
  // We check at runtime so the same code works on all variants.
  int coreCount = ESP.getChipCores();
  Serial.printf("[BOOT] Chip: %s, %d core(s), %d MHz\n",
                ESP.getChipModel(), coreCount, ESP.getCpuFreqMHz());

  if (coreCount >= 2) {
    dualCoreAvailable = true;

    // Create a FreeRTOS task pinned to Core 0.
    // xTaskCreatePinnedToCore(function, name, stack_size, parameter, priority, handle, core)
    //   - Priority 1 = low (WiFi stack is priority 19+, we don't want to starve it)
    //   - Core 0 = the "other" core (setup/loop run on Core 1)
    xTaskCreatePinnedToCore(
      httpBackgroundTask,       // Function to run
      "HttpTask",               // Human-readable name (for debugging)
      HTTP_TASK_STACK_SIZE,     // Stack size in bytes (8KB)
      NULL,                     // Parameter to pass (we use global httpReq instead)
      1,                        // Priority (1 = low, below WiFi stack)
      NULL,                     // Task handle (we don't need to reference it later)
      0                         // Core 0
    );

    Serial.println("[BOOT] Dual-core mode: HTTP calls run on Core 0 (background)");
  } else {
    dualCoreAvailable = false;
    Serial.println("[BOOT] Single-core mode: HTTP calls run inline (may cause brief display freezes)");
  }

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
  // --- If in portal mode, only run the portal ---
  if (portalRunning) {
    portalLoop();
    delay(10);
    return;
  }

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
  // Shows "!" in the status bar when something is wrong with API calls.
  bool recentFail = (millis() - lastApiFailMs) < API_WARN_WINDOW_MS;
  bool noLimitTooLong = (fixValid && currentSpeedLimitMph <= 0 &&
                         firstFixMs != 0 && (millis() - firstFixMs) > API_NO_LIMIT_WARN_MS);
  bool apiWarn = recentFail || noLimitTooLong || apiRateLimited;

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

  // --- Check for background HTTP results (dual-core mode) ---
  // If the background task finished an API call, pick up the result.
  if (dualCoreAvailable && httpReq.resultReady) {
    httpReq.resultReady = false;   // Acknowledge the result

    if (httpReq.httpCode == 429) {
      // Rate limited! Start exponential backoff.
      handleRateLimit();
      lastApiFailMs = millis();
    } else if (httpReq.resultLimit > 0) {
      // Got a valid speed limit — apply smoothing
      int smoothed = smoothSpeedLimit(httpReq.resultLimit);
      currentSpeedLimitMph = smoothed;
      lastGoodLimitMs = millis();
      lastApiLat = httpReq.lat;
      lastApiLon = httpReq.lon;
      haveApiPos = true;
      resetRateLimitBackoff();   // Successful call, reset backoff
    } else {
      lastApiFailMs = millis();
    }

    lastApiMs = millis();
  }

  // --- Should we request an API call? ---
  bool timeOk = (millis() - lastApiMs) >= apiMinIntervalMs;
  bool moveOk = true;
  if (haveApiPos) {
    moveOk = distanceMeters(lat, lon, lastApiLat, lastApiLon) >= apiMinMoveM;
  }
  bool rateLimitOk = !apiRateLimited || (millis() >= apiBackoffUntil);

  if (timeOk && moveOk && wifiReady && rateLimitOk) {
    if (dualCoreAvailable) {
      // --- DUAL-CORE: hand off to background task (non-blocking!) ---
      // We only start a new request if the background task isn't already busy.
      if (!httpReq.requestNeeded) {
        prepareHttpRequest(lat, lon);
        lastApiMs = millis();   // Mark that we've initiated a call
      }
    } else {
      // --- SINGLE-CORE: make the API call inline (blocks briefly) ---
      int httpCode = 0;
      int rawLimit = fetchSpeedLimit(lat, lon, &httpCode);

      if (httpCode == 429) {
        handleRateLimit();
        lastApiFailMs = millis();
      } else if (rawLimit > 0) {
        int smoothed = smoothSpeedLimit(rawLimit);
        currentSpeedLimitMph = smoothed;
        lastGoodLimitMs = millis();
        lastApiLat = lat;
        lastApiLon = lon;
        haveApiPos = true;
        resetRateLimitBackoff();
      } else {
        lastApiFailMs = millis();
      }

      lastApiMs = millis();
    }
  }

  // --- Is the speed limit still fresh enough to display? ---
  bool haveRecentLimit = (currentSpeedLimitMph > 0) &&
                         ((millis() - lastGoodLimitMs) < LIMIT_HOLD_MS);

  if (!haveRecentLimit) {
    drawCurrentSpeed(speedMph);   // Still show your speed even without a limit
    drawMain("--", C_GRAY());
    delay(25);
    return;
  }

  // --- Update overspeed state and display ---
  overState = computeOverState(speedMph, currentSpeedLimitMph);
  drawCurrentSpeed(speedMph);
  drawMain(String(currentSpeedLimitMph), colorForOverState(overState));

  delay(25);
}
