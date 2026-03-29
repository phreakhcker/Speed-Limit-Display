// =============================================
// CONFIGURATION — All tunable settings in one place
// =============================================
// This file is safe to push to GitHub (no secrets here).
// Secrets (WiFi password, API key) live in config_secrets.h
//
// If you want to change pin wiring, timing, thresholds, or
// display settings, do it here — not in the main .ino file.

#ifndef CONFIG_H
#define CONFIG_H

// =====================
// HARDWARE PINS
// =====================

// --- OLED Display (SPI) ---
// These are the wires connecting your ESP32 to the OLED screen.
// SPI = "Serial Peripheral Interface" — a fast way to talk to displays.
#define SCREEN_W    128       // Screen width in pixels
#define SCREEN_H    128       // Screen height in pixels
#define OLED_CS     7         // Chip Select — tells the display "I'm talking to you"
#define OLED_DC     2         // Data/Command — tells display if we're sending a command or pixel data
#define OLED_RST    3         // Reset — restarts the display on boot
#define SPI_MOSI    4         // Master Out Slave In — the data wire (ESP32 sends pixels here)
#define SPI_SCLK    6         // Serial Clock — timing signal for SPI

// --- GPS Module (UART / Serial) ---
// UART = "Universal Asynchronous Receiver Transmitter" — how the GPS talks to the ESP32.
// GPS TX wire -> ESP32 RX pin (GPS sends data TO the ESP32)
// GPS RX wire -> ESP32 TX pin (ESP32 sends commands TO the GPS)
static const int GPS_RX_PIN = 20;    // ESP32 receives GPS data on this pin
static const int GPS_TX_PIN = 21;    // ESP32 sends commands to GPS on this pin

// --- Button ---
// One button does everything: short press = brightness, long press = menu.
// Wiring: one leg to BTN_PIN, other leg to GND. We use INPUT_PULLUP
// (the ESP32 has a built-in resistor that keeps the pin HIGH when not pressed).
static const int BTN_PIN = 10;

// =====================
// GPS SETTINGS
// =====================

// Baud rate = how fast data flows over the serial wire (bits per second).
// 9600 is the GPS default but too slow for 10Hz updates.
// 38400 gives plenty of bandwidth for 10 messages per second.
static const uint32_t GPS_TARGET_BAUD = 38400;

// How many GPS points to buffer for the Snap-to-Roads API.
// More points = better road matching, but uses more memory and bigger API request.
// 5 is a sweet spot: enough history for TomTom to figure out your trajectory.
static const int GPS_BUFFER_SIZE = 5;

// Grace period after boot — don't show errors while GPS is still getting its first fix.
// Cold start can take 30-60 seconds, so we wait 60s before complaining.
static const uint32_t GPS_GRACE_MS = 60000;

// After losing GPS fix, keep showing the last known data for this long.
// Covers brief signal drops (tunnels, overpasses).
static const uint32_t GPS_LOST_HOLD_MS = 15000;

// GPS icon blink rate when searching for satellites (milliseconds per blink).
static const uint32_t GPS_BLINK_MS = 500;

// Speeds below this are treated as "stopped" (GPS is noisy at low speeds).
// Without this, GPS might say 2mph when you're parked, which could
// trigger false overspeed warnings in a parking lot.
static const int SPEED_DEAD_ZONE_MPH = 3;

// =====================
// API SETTINGS
// =====================

// Minimum time between TomTom API calls (milliseconds).
// Lower = more frequent updates, but burns through your free 2,500/day quota faster.
// At 4 seconds: ~900 calls/hour = ~2.7 hours of driving per day on free tier.
// At 8 seconds: ~450 calls/hour = ~5.5 hours/day.
static const uint32_t API_DEFAULT_INTERVAL_MS = 5000;

// Minimum distance you must move before making another API call (meters).
// Prevents wasting API calls when stopped at a red light.
// At 60mph you travel ~27m per second, so 20m means roughly 1 call per second
// at highway speeds (but the time interval above is the real limiter).
static const float API_DEFAULT_MOVE_M = 20.0f;

// How long to keep showing a speed limit after the last successful API response.
// If we can't reach the API for this long, we show "--" instead of stale data.
static const uint32_t LIMIT_HOLD_MS = 180000;    // 3 minutes

// Show a warning icon if the API has been failing for this long.
static const uint32_t API_WARN_WINDOW_MS = 30000; // 30 seconds

// Show a warning if we have GPS but no speed limit data after this long.
static const uint32_t API_NO_LIMIT_WARN_MS = 20000; // 20 seconds

// HTTP request timeout — how long to wait for TomTom's server to respond.
// Shorter = less GPS data loss during API calls. 4 seconds is a good balance.
static const int HTTP_TIMEOUT_MS = 4000;

// JSON parsing buffer size (bytes). The TomTom response is usually ~500-800 bytes.
// 1536 gives us comfortable headroom without wasting precious ESP32 RAM.
static const int JSON_BUFFER_SIZE = 1536;

// Snap-to-Roads search radius (meters). Smaller = more precise but might miss
// if your GPS position is far from the actual road.
static const int SNAP_RADIUS_M = 30;

// Reverse Geocode fallback radius. Smaller than the original 100m to reduce
// false matches on nearby parallel roads.
static const int GEOCODE_RADIUS_M = 25;

// =====================
// SPEED LIMIT SMOOTHING
// =====================
// These settings prevent the display from jumping between random speed limits.

// How many recent API results to remember for smoothing.
static const int LIMIT_HISTORY_SIZE = 5;

// If a new speed limit differs from the current by more than this,
// we require confirmation (multiple readings) before switching.
// Example: if you're showing 65 and the API says 25, that's a 40mph jump —
// probably a GPS glitch matching a side road. We wait for a second reading.
static const int LIMIT_JUMP_THRESHOLD_MPH = 15;

// How many times we need to see the same "big jump" value before accepting it.
// 2 means: if we see 25 twice in a row, it's probably real (you turned off the highway).
static const int LIMIT_CONFIRM_COUNT = 2;

// =====================
// BUTTON TIMING
// =====================

// Debounce = ignore rapid electrical noise when the button is pressed/released.
// Without this, one press might register as 3-4 presses.
static const uint32_t BTN_DEBOUNCE_MS = 35;

// How long to hold the button for a "long press" (enters menu / selects option).
static const uint32_t BTN_LONG_MS = 1200;

// How long to hold for a "very long press" (exits the menu).
static const uint32_t BTN_VLONG_MS = 2500;

// =====================
// BRIGHTNESS
// =====================
// Software dimming levels. 1.0 = full brightness, lower = dimmer.
// We scale the color values instead of using hardware PWM on the display.
// 8 levels gives a good range from "daylight readable" to "not blinding at night".
static const float BRIGHT_LEVELS[] = { 1.00f, 0.85f, 0.70f, 0.55f, 0.40f, 0.25f, 0.15f, 0.08f };
static const int BRIGHT_LEVEL_COUNT = sizeof(BRIGHT_LEVELS) / sizeof(BRIGHT_LEVELS[0]);

// =====================
// WIFI
// =====================

// How long to wait for WiFi to connect before giving up (per attempt).
// Non-blocking: we check periodically, we don't freeze the whole device.
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;

// How long to wait after a failed WiFi connection before trying again.
static const uint32_t WIFI_RETRY_INTERVAL_MS = 5000;

// =====================
// WATCHDOG
// =====================
// The watchdog timer reboots the ESP32 if the main loop stops running.
// This prevents the device from permanently freezing. The main loop
// must "pet the watchdog" (reset the timer) within this many seconds.
static const int WATCHDOG_TIMEOUT_SEC = 10;

#endif
