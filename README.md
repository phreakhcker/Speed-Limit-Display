# Speed Limit Display

A real-time speed limit display built with an ESP32, GPS module, and OLED screen. Shows the current road's speed limit with color-coded overspeed warnings (green/yellow/red).

Uses the TomTom Snap-to-Roads API for accurate road matching — sends your heading and speed so it picks the correct road, even near highway ramps and parallel roads.

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32 Mini (any variant: original, S2, S3, C3) |
| GPS Module | Any NMEA-compatible (NEO-6M, NEO-7M, BN-220, etc.) |
| Display | SSD1351 128x128 SPI OLED |
| Button | One momentary pushbutton |
| Power | USB or 5V supply |

## Wiring

| ESP32 Pin | Connects To |
|-----------|-------------|
| GPIO 20 | GPS TX |
| GPIO 21 | GPS RX |
| GPIO 7 | OLED CS |
| GPIO 2 | OLED DC |
| GPIO 3 | OLED RST |
| GPIO 4 | OLED MOSI (SDA) |
| GPIO 6 | OLED SCLK (SCL) |
| GPIO 10 | Button (other leg to GND) |

Pin assignments can be changed in `config.h`.

## Features

- **Snap-to-Roads API** — sends GPS heading + speed for accurate road matching
- **GPS point buffering** — 5-point trajectory for better map matching
- **Speed limit smoothing** — rejects false readings from nearby roads
- **Overspeed alerts** — green (ok), yellow (close to limit), red (over limit) with hysteresis
- **Dual-core HTTP** — API calls on Core 0, GPS/display on Core 1 (no display freezes)
- **Non-blocking WiFi** — GPS keeps working during WiFi reconnects
- **API rate limiting** — exponential backoff on HTTP 429, daily quota tracking
- **Watchdog timer** — auto-reboots if the device freezes
- **One-button menu** — adjust brightness, alert thresholds, API timing, GPS reset
- **Persistent settings** — brightness and thresholds saved across reboots

## Setup

### 1. Install Arduino Libraries

Open Arduino IDE, go to **Sketch > Include Library > Manage Libraries** and install:

- **TinyGPSPlus** (by Mikal Hart)
- **ArduinoJson** (by Benoit Blanchon, version 6.x)
- **Adafruit GFX Library**
- **Adafruit SSD1351 Library**

### 2. Get a TomTom API Key (free)

1. Go to [developer.tomtom.com](https://developer.tomtom.com)
2. Create a free account
3. Go to Dashboard > My Apps > Add New App
4. Copy your API key (free tier = 2,500 requests/day)

### 3. Configure Credentials

1. In the `SpeedLimitDisplay/` folder, copy `config_secrets_example.h` to `config_secrets.h`
2. Edit `config_secrets.h` with your WiFi name, password, and TomTom API key
3. `config_secrets.h` is in `.gitignore` — it will never be uploaded to GitHub

### 4. Upload to ESP32

1. Open `SpeedLimitDisplay/SpeedLimitDisplay.ino` in Arduino IDE
2. Select your ESP32 board in **Tools > Board**
3. Click **Upload**

### 5. Serial Monitor (optional, for debugging)

Open **Tools > Serial Monitor** at 115200 baud. Type:
- **`d`** — full debug status dump
- **`r`** — reboot the device

## Button Controls

| Action | Normal Mode | Menu Mode |
|--------|------------|-----------|
| Short press | Cycle brightness | Next menu item |
| Long press (1.2s) | Enter menu | Change selected setting |
| Very long press (2.5s) | — | Exit menu |

## Configuration

All tunable settings (pins, timing, thresholds) are in `config.h`. Secrets (WiFi, API key) are in `config_secrets.h`.

## Project Structure

```
Speed-Limit-Display/
├── SpeedLimitDisplay/              # Arduino sketch folder
│   ├── SpeedLimitDisplay.ino       # Main code
│   ├── config.h                    # All tunable settings
│   ├── config_secrets.h            # Your credentials (git-ignored)
│   └── config_secrets_example.h    # Template for credentials
├── .gitignore
├── CONTRIBUTING.md                 # How to contribute
├── LICENSE
└── README.md                       # This file
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to set up your environment, create branches, and submit changes.

## License

See [LICENSE](LICENSE) for details.
