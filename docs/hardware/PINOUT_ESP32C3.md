# Pinout Map — Current Prototype

## Components

| Component | Model | Interface | Notes |
|-----------|-------|-----------|-------|
| Microcontroller | ESP32-C3 Mini | — | Single core, WiFi, BLE, RISC-V |
| GPS Module | GT-U7 (Goouuu Tech) | UART | u-blox NEO-6M compatible. Uses UBX protocol (NOT PMTK) |
| Display | SSD1351 128x128 OLED | SPI | 1.5" RGB OLED, 65K colors |
| Button | Momentary pushbutton | GPIO (INPUT_PULLUP) | Between pin and GND |

---

## ESP32-C3 Mini Pinout

```
                    ESP32-C3 Mini
                  ┌───────────────┐
                  │    USB-C      │
                  │  ┌─────────┐  │
            3V3 ──┤  │         │  ├── 5V
            GND ──┤  │ ESP32-C3│  ├── GND
          GPIO0 ──┤  │         │  ├── GPIO10  ← BUTTON
          GPIO1 ──┤  │         │  ├── GPIO9
      SDA GPIO2 ──┤  │         │  ├── GPIO8
          GPIO3 ──┤  │         │  ├── GPIO21  → GPS RX (ESP TX)
  SPI MOSI GPIO4 ──┤  │         │  ├── GPIO20  ← GPS TX (ESP RX)
          GPIO5 ──┤  │         │  ├── GPIO7   → OLED CS
  SPI SCLK GPIO6 ──┤  └─────────┘  ├── GPIO1
                  └───────────────┘

  Note: Actual pin layout may vary by manufacturer.
  Always check YOUR specific board's silkscreen labels.
```

---

## Wiring Connections

### Display (SSD1351 128x128 SPI OLED)

```
  ESP32-C3          SSD1351 OLED
  ────────          ────────────
  GPIO 4  ────────→ SDA (MOSI)     Data line
  GPIO 6  ────────→ SCL (SCLK)     Clock line
  GPIO 7  ────────→ CS              Chip Select
  GPIO 2  ────────→ DC              Data/Command
  GPIO 3  ────────→ RST             Reset
  3V3     ────────→ VCC             Power (3.3V)
  GND     ────────→ GND             Ground
```

### GPS Module (GT-U7)

```
  ESP32-C3          GT-U7 GPS
  ────────          ─────────
  GPIO 20 ←──────── TX              GPS sends data TO ESP32
  GPIO 21 ────────→ RX              ESP32 sends commands TO GPS
  3V3     ────────→ VCC             Power (3.3V — check if module needs 5V)
  GND     ────────→ GND             Ground

  ⚠️  IMPORTANT: The GT-U7 may need 5V power (check your module).
     If so, connect VCC to 5V, but the TX/RX data lines are still
     3.3V logic and safe to connect directly to the ESP32.
```

### Button

```
  ESP32-C3          Button
  ────────          ──────
  GPIO 10 ────────→ Pin 1
  GND     ────────→ Pin 2

  No external resistor needed — we use INPUT_PULLUP
  (the ESP32 has a built-in pull-up resistor on the pin).
```

---

## Complete Wiring Diagram

```
                           ┌──────────────┐
                           │  SSD1351     │
                           │  128x128     │
                           │  OLED        │
                           │              │
                    ┌──────┤ SDA     VCC ├─── 3V3
                    │ ┌────┤ SCL     GND ├─── GND
                    │ │ ┌──┤ CS          │
                    │ │ │┌─┤ DC          │
                    │ │ ││┌┤ RST         │
                    │ │ │││└──────────────┘
                    │ │ │││
  ┌─────────────────┼─┼─┼┼┼────────────────┐
  │ ESP32-C3 Mini   │ │ │││                │
  │                 │ │ │││                │
  │  GPIO 4 ───────┘ │ │││  GPIO 20 ◄──── │ ──── GPS TX
  │  GPIO 6 ──────────┘ │││  GPIO 21 ────► │ ──── GPS RX
  │  GPIO 7 ─────────────┘││                │
  │  GPIO 2 ──────────────┘│                │
  │  GPIO 3 ───────────────┘                │
  │                                         │
  │  GPIO 10 ─────────────── BUTTON ── GND  │
  │                                         │
  │  3V3 ──────────────────── GPS VCC       │
  │  GND ──────────────────── GPS GND       │
  │  5V (USB) ── Power In                   │
  └─────────────────────────────────────────┘
```

---

## Pin Summary Table

| ESP32-C3 Pin | Connected To | Direction | Function |
|-------------|-------------|-----------|----------|
| GPIO 2 | OLED DC | Output | Data/Command select |
| GPIO 3 | OLED RST | Output | Display reset |
| GPIO 4 | OLED SDA (MOSI) | Output | SPI data to display |
| GPIO 6 | OLED SCL (SCLK) | Output | SPI clock |
| GPIO 7 | OLED CS | Output | SPI chip select |
| GPIO 10 | Button | Input (pullup) | Menu/brightness button |
| GPIO 20 | GPS TX | Input | Receive GPS NMEA data |
| GPIO 21 | GPS RX | Output | Send UBX commands to GPS |
| 3V3 | OLED VCC, GPS VCC | Power | 3.3V supply |
| GND | OLED GND, GPS GND, Button | Power | Common ground |
| 5V (USB) | — | Power | USB power input |

---

## GT-U7 GPS Module Pinout

```
  GT-U7 Module
  ┌─────────────────────┐
  │  ┌───┐              │
  │  │GPS│  Ceramic      │
  │  │Ant│  Antenna      │
  │  └───┘              │
  │                     │
  │  VCC  TX  RX  GND   │
  └──┬───┬───┬───┬──────┘
     │   │   │   │
     5V  │   │   GND
         │   │
    ESP  │   ESP
    RX   │   TX
   (20)  │  (21)
         │
   (data flows FROM GPS TO ESP32)
```

### GT-U7 Specs
- **Chip:** u-blox NEO-6M compatible
- **Default baud rate:** 9600
- **Protocol:** NMEA 0183 + UBX binary
- **Update rate:** 1Hz default, up to 5Hz (10Hz on some clones — may not be stable)
- **Antenna:** Built-in ceramic patch antenna
- **Cold start:** ~27 seconds
- **Hot start:** ~1 second
- **Power:** 3.3V-5V (check your specific module)

### ⚠️ Important Note About GPS Commands

The GT-U7 uses **u-blox UBX protocol**, NOT MediaTek PMTK commands.
If you see "PMTK" in the code, those commands won't work on your module.
The code must use UBX binary commands to configure:
- Update rate (default 1Hz → target 5Hz)
- Navigation mode (pedestrian → automotive)
- Baud rate (9600 → 38400)
- Message filtering (reduce to RMC+GGA only)

---

## ESP32-C3 Limitations for This Project

| Feature | ESP32-C3 | ESP32-S3 (product target) |
|---------|----------|---------------------------|
| CPU Cores | 1 (RISC-V) | 2 (Xtensa) |
| Clock Speed | 160 MHz | 240 MHz |
| RAM | 400 KB | 512 KB + optional 8MB PSRAM |
| WiFi | Yes | Yes |
| Bluetooth | BLE only | BLE + Classic |
| USB | CDC (serial only) | OTG (can act as USB device) |
| SPI interfaces | 1 usable | 2 usable |
| Background HTTP | ❌ Falls back to inline | ✅ Runs on Core 0 |
| Touch display | Works but tight on RAM | ✅ Plenty of resources |

**Recommendation:** The ESP32-C3 is fine for prototyping, but the product PCB should use an **ESP32-S3-WROOM-1** module for dual-core performance and better display handling.
