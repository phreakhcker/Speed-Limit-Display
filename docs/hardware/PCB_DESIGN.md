# Custom PCB Design — Speed Limit Display v1

All-in-one board: ESP32-S3 + GPS + Touchscreen TFT + Power + Controls.

Designed for manufacturing by PCBWay.

---

## Design Goals

1. Everything on one board (no breakout modules, no jumper wires)
2. Small enough for a car-mount enclosure (~60mm x 45mm target)
3. USB-C for power and programming
4. Touchscreen TFT connector (FPC ribbon cable)
5. GPS with on-board ceramic patch antenna OR u.FL connector for external antenna
6. Rotary encoder + push button for tactile control
7. Optional piezo buzzer footprint (populate or leave empty)
8. 2-layer PCB (keeps cost low at PCBWay)

---

## Component Selection (BOM)

### Core Components

| Ref | Component | Part Number | Package | Qty | Est. Cost | Notes |
|-----|-----------|-------------|---------|-----|-----------|-------|
| U1 | ESP32-S3-WROOM-1-N8 | ESP32-S3-WROOM-1-N8 | Module | 1 | $3.00 | Dual core, 8MB flash, WiFi+BLE, pre-certified |
| U2 | GPS Receiver | ATGM336H or MAX-M10S | Module | 1 | $3.50 | NMEA + UBX compatible, built-in antenna |
| U3 | 3.3V LDO Regulator | AMS1117-3.3 | SOT-223 | 1 | $0.10 | 5V USB → 3.3V for ESP32 + GPS |
| U4 | USB-C Controller | — | — | — | — | Built into ESP32-S3 (native USB) |

### Display

| Ref | Component | Part Number | Package | Qty | Est. Cost | Notes |
|-----|-----------|-------------|---------|-----|-----------|-------|
| LCD1 | 2.0" IPS TFT with Touch | ST7789 + CST816S | 24-pin FPC | 1 | $7.00 | 240x320, capacitive touch, I2C touch controller |
| J1 | FPC Connector 24-pin | 0.5mm pitch | SMD | 1 | $0.30 | Bottom contact, flip-lock |

**Alternative displays (in order of preference):**

| Size | Controller | Resolution | Touch | Price | Notes |
|------|-----------|------------|-------|-------|-------|
| 2.0" IPS | ST7789 | 240x320 | CST816S capacitive | $7 | **Recommended** — good size, great sunlight readability |
| 1.69" IPS | ST7789 | 240x280 | CST816S capacitive | $6 | Slightly smaller, rounded corners look nice |
| 2.4" IPS | ILI9341 | 240x320 | FT6236 capacitive | $9 | Bigger but may be too large for car mount |
| 1.28" Round | GC9A01 | 240x240 | CST816S capacitive | $6 | Cool look but limited info display area |

### GPS Antenna Options

| Option | Component | Notes |
|--------|-----------|-------|
| A (recommended) | On-board ceramic patch antenna | 25x25mm, needs ground plane keep-out. Simplest, no external parts. |
| B (alternative) | u.FL connector + external antenna | Better reception, user can mount antenna on dashboard. Adds $1 + antenna cost. |
| C (both) | Ceramic patch + u.FL with 0-ohm select | Most flexible. Populate one or the other. |

### Power

| Ref | Component | Value | Package | Qty | Notes |
|-----|-----------|-------|---------|-----|-------|
| J2 | USB-C Connector | USB 2.0 | SMD 16-pin | 1 | Power + data (ESP32-S3 native USB) |
| U3 | AMS1117-3.3 | 3.3V 1A | SOT-223 | 1 | 5V → 3.3V |
| C1 | Input capacitor | 10uF 16V | 0805 | 1 | USB 5V filtering |
| C2 | Output capacitor | 10uF 16V | 0805 | 1 | 3.3V rail stability |
| C3 | Decoupling cap | 100nF | 0402 | 4 | One per IC power pin |
| D1 | ESD protection | USBLC6-2SC6 | SOT-23-6 | 1 | USB data line protection |
| F1 | Resettable fuse | 500mA PTC | 0805 | 1 | USB overcurrent protection |

### User Interface

| Ref | Component | Part Number | Package | Qty | Notes |
|-----|-----------|-------------|---------|-----|-------|
| SW1 | Rotary encoder with push | EC11 compatible | Through-hole | 1 | Rotate = navigate, push = select |
| BZ1 | Piezo buzzer | 5V passive | 12mm through-hole | 1 | Optional — don't populate for silent version |
| LED1 | Status LED | 0805 green | 0805 | 1 | Power/status indicator |
| R1 | LED resistor | 1K | 0402 | 1 | For LED1 |
| SW2 | Reset button | Tactile 6x6mm | SMD | 1 | Accessible from outside enclosure |
| SW3 | Boot button | Tactile 6x6mm | SMD | 1 | Hold during reset for firmware flash mode |

### Passive Components

| Ref | Component | Value | Package | Qty | Notes |
|-----|-----------|-------|---------|-----|-------|
| R2, R3 | USB data resistors | 5.1K | 0402 | 2 | USB-C CC pins (tells charger we want 5V) |
| R4, R5 | I2C pull-ups | 4.7K | 0402 | 2 | For touch controller I2C bus |
| C4 | ESP32 boot cap | 100nF | 0402 | 1 | On EN pin for clean boot |
| R6 | Boot pull-up | 10K | 0402 | 1 | Keep GPIO0 high during normal boot |

---

## Schematic — Block Diagram

```
                                    ┌──────────────────────┐
   USB-C ──→ 5V ──→ AMS1117 ──→ 3V3 ──┤                      │
      │            (LDO)           │  ESP32-S3-WROOM-1     │
      │                            │                      │
      │   D+/D- ──────────────────┤ GPIO 19/20 (USB)      │
      │                            │                      │
      │                            │  SPI Bus:             │
      │                            │   GPIO 11 (MOSI) ────┼──→ LCD SDA
      │                            │   GPIO 12 (SCLK) ────┼──→ LCD SCL
      │                            │   GPIO 10 (CS)   ────┼──→ LCD CS
      │                            │   GPIO 9  (DC)   ────┼──→ LCD DC
      │                            │   GPIO 8  (RST)  ────┼──→ LCD RST
      │                            │   GPIO 13 (BL)   ────┼──→ LCD Backlight
      │                            │                      │
      │                            │  I2C Bus (Touch):     │
      │                            │   GPIO 1  (SDA)  ────┼──→ Touch SDA
      │                            │   GPIO 2  (SCL)  ────┼──→ Touch SCL
      │                            │   GPIO 3  (INT)  ←───┼─── Touch INT
      │                            │   GPIO 4  (RST)  ────┼──→ Touch RST
      │                            │                      │
      │                            │  UART (GPS):          │
      │                            │   GPIO 17 (RX)   ←───┼─── GPS TX
      │                            │   GPIO 18 (TX)   ────┼──→ GPS RX
      │                            │                      │
      │                            │  Rotary Encoder:      │
      │                            │   GPIO 5  (A)    ←───┼─── Encoder A
      │                            │   GPIO 6  (B)    ←───┼─── Encoder B
      │                            │   GPIO 7  (SW)   ←───┼─── Encoder Push
      │                            │                      │
      │                            │  Misc:                │
      │                            │   GPIO 14 (BZ)   ────┼──→ Buzzer
      │                            │   GPIO 15 (LED)  ────┼──→ Status LED
      │                            │   GPIO 0  (BOOT) ←───┼─── Boot Button
      │                            │   EN     (RESET) ←───┼─── Reset Button
      │                            │                      │
      │                            └──────────────────────┘
      │
      │              ┌──────────────┐
      └──── 3V3 ────┤  GPS Module   │
                     │  ATGM336H    │
                     │              │
                     │  TX ─────────┼──→ ESP32 GPIO 17
                     │  RX ←────────┼─── ESP32 GPIO 18
                     │              │
                     │  ┌────────┐  │
                     │  │Ceramic │  │
                     │  │Antenna │  │
                     │  └────────┘  │
                     └──────────────┘
```

---

## Pin Assignment Table (Product PCB)

| ESP32-S3 GPIO | Function | Direction | Connected To |
|---------------|----------|-----------|-------------|
| GPIO 0 | Boot mode | Input | Boot button (with 10K pull-up) |
| GPIO 1 | I2C SDA | Bidirectional | Touch controller SDA |
| GPIO 2 | I2C SCL | Output | Touch controller SCL |
| GPIO 3 | Touch interrupt | Input | Touch INT (active low) |
| GPIO 4 | Touch reset | Output | Touch RST |
| GPIO 5 | Encoder A | Input (pullup) | Rotary encoder channel A |
| GPIO 6 | Encoder B | Input (pullup) | Rotary encoder channel B |
| GPIO 7 | Encoder button | Input (pullup) | Rotary encoder push switch |
| GPIO 8 | LCD Reset | Output | Display RST |
| GPIO 9 | LCD DC | Output | Display Data/Command |
| GPIO 10 | LCD CS | Output | Display Chip Select |
| GPIO 11 | SPI MOSI | Output | Display SDA (data) |
| GPIO 12 | SPI SCLK | Output | Display SCL (clock) |
| GPIO 13 | LCD Backlight | Output (PWM) | Display backlight control |
| GPIO 14 | Buzzer | Output (PWM) | Piezo buzzer (optional) |
| GPIO 15 | Status LED | Output | Power/activity LED |
| GPIO 17 | UART RX | Input | GPS TX (data from GPS) |
| GPIO 18 | UART TX | Output | GPS RX (commands to GPS) |
| GPIO 19 | USB D- | Bidirectional | USB-C data |
| GPIO 20 | USB D+ | Bidirectional | USB-C data |
| EN | Reset | Input | Reset button (with RC circuit) |

---

## PCB Layout Guidelines

### Board Specs

| Parameter | Value |
|-----------|-------|
| Board size | 60mm x 45mm (target) |
| Layers | 2 (top + bottom) |
| Copper weight | 1 oz |
| Min trace width | 0.2mm (8 mil) |
| Min via size | 0.3mm drill / 0.6mm pad |
| Board thickness | 1.6mm |
| Surface finish | HASL (lead-free) or ENIG |
| Solder mask | Black (looks professional) |
| Silkscreen | White |

### Layout Rules

1. **GPS antenna zone (CRITICAL)**
   - The ceramic patch antenna needs a **clear ground plane underneath** (no traces, no vias, no other copper on the layer below)
   - Keep the antenna at the **edge of the board** with nothing above it
   - Minimum **10mm keep-out** from antenna to any component
   - No ground pour directly under the antenna element (but ground plane around it is fine)

2. **Power routing**
   - USB 5V trace: minimum 0.5mm wide (carries up to 500mA)
   - 3.3V trace: minimum 0.3mm wide
   - Ground pour on bottom layer (solid copper fill connected to GND)
   - Keep AMS1117 input/output caps within 5mm of the regulator

3. **SPI display bus**
   - Keep SPI traces short and matched in length (under 50mm)
   - Route MOSI and SCLK as a pair, close together
   - Ground plane underneath SPI traces for return path

4. **USB-C**
   - Place USB-C connector at board edge
   - Keep D+/D- traces short, matched length, 90-ohm differential impedance
   - Place ESD protection within 10mm of connector

5. **Component placement (suggested)**
   ```
   ┌─────────────────────────────────────────────┐
   │  GPS Antenna                    FPC (LCD)    │
   │  ┌────────┐                    ┌──────────┐ │
   │  │        │                    │          │ │
   │  └────────┘                    └──────────┘ │
   │                                              │
   │  GPS Module      ESP32-S3-WROOM-1           │
   │  ┌──────┐       ┌──────────────────┐        │
   │  │ATGM  │       │                  │        │
   │  │336H  │       │    ESP32-S3      │        │
   │  └──────┘       │                  │        │
   │                  └──────────────────┘        │
   │                                              │
   │  [Buzzer]  [LED]  [Encoder]                  │
   │                                              │
   │  ┌──────┐ C1 C2  AMS1117                    │
   │  │USB-C │ ── ── ┌──────┐                    │
   │  └──────┘       └──────┘  [RST] [BOOT]     │
   └─────────────────────────────────────────────┘
   ```

---

## GPS Module Options for PCB

### Option 1: ATGM336H (Recommended for cost)
- **Price:** ~$2.50
- **Size:** 9.7 x 10.1 mm
- **Built-in antenna:** No (needs external ceramic patch or chip antenna)
- **Protocol:** NMEA + proprietary (similar to u-blox UBX)
- **Update rate:** Up to 10Hz
- **Good for:** Cheapest option, very small

### Option 2: Quectel L76K (Recommended for performance)
- **Price:** ~$4.00
- **Size:** 10.1 x 9.7 mm
- **Built-in antenna:** Optional (with L76K-M33)
- **Protocol:** NMEA + PMTK (MediaTek based)
- **Update rate:** Up to 10Hz
- **Good for:** Better sensitivity, wider protocol support

### Option 3: u-blox MAX-M10S (Premium)
- **Price:** ~$8.00
- **Size:** 9.7 x 10.1 mm
- **Built-in antenna:** No
- **Protocol:** NMEA + UBX
- **Update rate:** Up to 10Hz
- **Good for:** Best accuracy, most reliable, industry standard

### Antenna
- **Ceramic patch antenna:** 25x25x4mm, connects via trace to GPS RF input
- Add a **u.FL connector** as an alternative for users who want external antennas

---

## Bill of Materials (Full BOM)

| # | Qty | Component | Value/Part | Package | Est. Unit Cost |
|---|-----|-----------|-----------|---------|---------------|
| 1 | 1 | ESP32-S3-WROOM-1-N8 | 8MB Flash | Module | $3.00 |
| 2 | 1 | GPS Module | ATGM336H or Quectel L76K | Module | $2.50-4.00 |
| 3 | 1 | Ceramic GPS antenna | 25x25mm patch | SMD | $0.80 |
| 4 | 1 | AMS1117-3.3 | 3.3V 1A LDO | SOT-223 | $0.10 |
| 5 | 1 | USB-C connector | 16-pin SMD | SMD | $0.30 |
| 6 | 1 | USBLC6-2SC6 | ESD protection | SOT-23-6 | $0.25 |
| 7 | 1 | PTC resettable fuse | 500mA | 0805 | $0.05 |
| 8 | 1 | FPC connector 24-pin | 0.5mm pitch | SMD | $0.30 |
| 9 | 1 | Rotary encoder | EC11 w/ push | TH | $0.75 |
| 10 | 1 | Piezo buzzer | 5V passive | 12mm TH | $0.30 |
| 11 | 1 | Green LED | — | 0805 | $0.02 |
| 12 | 2 | Tactile switch | 6x6mm | SMD | $0.10 |
| 13 | 2 | Capacitor 10uF | 16V ceramic | 0805 | $0.05 |
| 14 | 4 | Capacitor 100nF | 16V ceramic | 0402 | $0.01 |
| 15 | 1 | Resistor 1K | — | 0402 | $0.01 |
| 16 | 2 | Resistor 5.1K | USB CC | 0402 | $0.01 |
| 17 | 2 | Resistor 4.7K | I2C pull-up | 0402 | $0.01 |
| 18 | 1 | Resistor 10K | Boot pull-up | 0402 | $0.01 |
| — | — | **PCB** | 2-layer, 60x45mm | — | $1.50 (at 100 qty) |
| — | — | **TOTAL (board only)** | | | **~$9.50-11.00** |
| — | — | **2.0" Touch TFT** | ST7789 + CST816S | FPC | $7.00 |
| — | — | **GRAND TOTAL** | | | **~$16.50-18.00** |

---

## Manufacturing Notes for PCBWay

### PCB Order Specs
- Layers: 2
- Dimensions: 60 x 45 mm
- Quantity: 5 (prototype) or 10-20 (small batch)
- Thickness: 1.6mm
- Surface finish: HASL lead-free
- Solder mask: Black
- Silkscreen: White
- Min hole size: 0.3mm
- Min trace/space: 0.2mm / 0.2mm

### Assembly (PCBA) Option
PCBWay offers assembly service — they solder the components for you:
- Upload BOM + pick-and-place file
- They source common parts (ESP32, passives, connectors)
- You may need to supply specialty parts (GPS module, FPC connector)
- Assembly adds ~$30-50 for small quantities (5-10 boards)

### Files to Provide PCBWay
1. **Gerber files** — the actual PCB design (generated from KiCad or EasyEDA)
2. **BOM (CSV)** — component list with part numbers and quantities
3. **Pick-and-place (CPL)** — tells the machine where to put each component
4. **Schematic (PDF)** — for reference during assembly review

---

## Design Tool Recommendation

**Use EasyEDA (Standard or Pro)** — it's free, web-based, and PCBWay owns it.

1. Go to [easyeda.com](https://easyeda.com)
2. Create the schematic using the component info above
3. Convert to PCB layout
4. Export Gerbers directly to PCBWay with one click

EasyEDA has built-in footprints for ESP32-S3-WROOM-1, AMS1117, USB-C connectors,
and most passives. The GPS module may need a custom footprint from the datasheet.

---

## Next Steps

1. **Choose final display** — order a 2.0" ST7789 touch TFT to test with prototype
2. **Choose final GPS module** — ATGM336H (cheapest) or Quectel L76K (better)
3. **Create schematic in EasyEDA** using pin assignments and BOM above
4. **Route the PCB** following the layout guidelines
5. **Generate Gerbers** and submit to PCBWay
6. **Order components** from LCSC (PCBWay's preferred parts supplier)
