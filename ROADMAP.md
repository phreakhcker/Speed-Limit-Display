# Speed Limit Display — Product Roadmap

Full plan from working prototype to selling a finished product.

---

## Phase 1 — Code Foundation (DONE)

Everything already completed in Rev 4.1.

- [x] Snap-to-Roads API with heading + speed
- [x] GPS point buffering (5-point trajectory)
- [x] Speed limit smoothing (outlier rejection)
- [x] 38400 baud GPS (10Hz support)
- [x] Non-blocking WiFi
- [x] Low-speed dead zone
- [x] Watchdog timer
- [x] Credential separation (config_secrets.h)
- [x] Configurable settings (config.h)

## Phase 2 — Reliability (DONE)

- [x] Dual-core background HTTP (Core 0)
- [x] API rate limit handling (HTTP 429 + exponential backoff)
- [x] Daily API quota tracking with warnings
- [x] WiFi exponential backoff on reconnect
- [x] WiFi disconnect/reconnect logging

## Phase 3 — Field Testing & Bug Fixes

Real-world driving to validate all the Phase 1-2 improvements.

- [ ] 3.1 Flash Rev 4.1 to ESP32 and bench test (power on, GPS lock, WiFi connect)
- [ ] 3.2 Drive test: suburban roads (25-45 mph zones, frequent limit changes)
- [ ] 3.3 Drive test: highway (65-75 mph, parallel frontage roads)
- [ ] 3.4 Drive test: rural (spotty WiFi/GPS, long stretches between limit signs)
- [ ] 3.5 Review serial debug output — verify Snap-to-Roads vs Reverse Geocode usage
- [ ] 3.6 Log speed limit accuracy: note what the display shows vs actual posted signs
- [ ] 3.7 Fix any bugs found during testing
- [ ] 3.8 Tune config.h values based on real-world results (API interval, move threshold, smoothing)

## Phase 4 — Display & UI Improvements

Better screen, more information, polished user experience.

- [ ] 4.1 Evaluate display upgrade: 2.0" IPS TFT (ST7789, 240x320) — better sunlight readability
- [ ] 4.2 Show BOTH current speed AND speed limit on screen (speed on top, limit below)
- [ ] 4.3 Add road name display (TomTom returns the road name in the API response)
- [ ] 4.4 Speed limit change animation (brief flash/highlight when the limit changes)
- [ ] 4.5 Add "approaching limit" indicator (speed bar or gauge that fills up)
- [ ] 4.6 Night mode: auto-dim based on ambient light sensor (optional LDR add-on)
- [ ] 4.7 Startup animation / splash screen with version number
- [ ] 4.8 Low battery / power loss indicator (if running on battery)

## Phase 5 — Input & Controls

Replace single button with something more intuitive.

- [ ] 5.1 Add rotary encoder with push button (rotate = scroll, push = select)
- [ ] 5.2 Redesign menu system for rotary encoder navigation
- [ ] 5.3 Add "quick settings" — single rotate to adjust brightness without entering menu
- [ ] 5.4 (Optional) Evaluate capacitive touch display for future "Pro" version

## Phase 6 — Audio Alerts

Optional sound feedback for overspeed.

- [ ] 6.1 Add piezo buzzer for overspeed beep
- [ ] 6.2 Make alert configurable: off / single beep / continuous
- [ ] 6.3 Add alert volume levels (PWM duty cycle)
- [ ] 6.4 Save audio preferences to NVS
- [ ] 6.5 (Optional) Evaluate small speaker for voice alerts ("Speed limit: 65")

## Phase 7 — WiFi Setup Portal

First-boot experience so users don't need to edit code.

- [ ] 7.1 On first boot (no saved WiFi), create a WiFi hotspot ("SpeedLimit-Setup")
- [ ] 7.2 Serve a web page where user enters WiFi name, password, and TomTom API key
- [ ] 7.3 Save credentials to NVS and reboot into normal mode
- [ ] 7.4 Add "Factory Reset" option in menu (clears saved WiFi/API key, returns to setup mode)
- [ ] 7.5 Add menu option to re-enter WiFi setup without full reset

## Phase 8 — Multi-WiFi & Phone Hotspot

Use the device anywhere, not just near home WiFi.

- [ ] 8.1 Store up to 5 WiFi networks in NVS
- [ ] 8.2 Auto-scan and connect to the strongest known network
- [ ] 8.3 Add "Add WiFi" option to the setup portal
- [ ] 8.4 Document how to use phone hotspot as the WiFi source (primary use case for driving)

## Phase 9 — OTA Firmware Updates

Update the device over WiFi without plugging in USB.

- [ ] 9.1 Add ArduinoOTA support
- [ ] 9.2 Add menu option: "Check for Updates"
- [ ] 9.3 Host firmware binary on GitHub Releases
- [ ] 9.4 Device downloads and flashes new firmware over WiFi
- [ ] 9.5 Add version check (don't re-flash if already current)

## Phase 10 — Hardware Design (Custom PCB)

Move from breadboard to a real board. **This is the PCBWay phase.**

- [ ] 10.1 Choose final ESP32 variant for product (recommend: ESP32-S3)
- [ ] 10.2 Choose final display (recommend: 2.0" IPS TFT ST7789)
- [ ] 10.3 Choose final GPS module (recommend: BN-220 or BN-880 for antenna quality)
- [ ] 10.4 Design schematic in KiCad or EasyEDA
    - ESP32-S3 module (WROOM or similar)
    - GPS module footprint + antenna keep-out zone
    - Display connector (FPC or pin header)
    - USB-C for power + programming
    - Rotary encoder footprint
    - Buzzer footprint (optional, populate or not)
    - Power regulation (5V USB → 3.3V LDO)
    - Status LED
    - Reset button
    - Boot button (for initial programming)
- [ ] 10.5 PCB layout — 2-layer board, keep GPS antenna area clear of copper
- [ ] 10.6 Generate Gerber files
- [ ] 10.7 Order prototype PCBs from PCBWay (5-10 boards)
- [ ] 10.8 Order components (BOM — Bill of Materials)
- [ ] 10.9 Solder and assemble first prototype board
- [ ] 10.10 Test prototype board — verify all connections, flash firmware
- [ ] 10.11 Rev 2 PCB if changes needed

## Phase 11 — Enclosure Design

A case that looks like a product, not a science project.

- [ ] 11.1 Design 3D-printed enclosure (Fusion 360 or similar)
    - Display window cutout
    - Rotary encoder knob hole
    - USB-C port access
    - GPS antenna area (keep plastic thin or add ceramic antenna)
    - Mounting options: suction cup mount, vent clip, dash pad
- [ ] 11.2 Print prototype enclosure on FDM printer
- [ ] 11.3 Test fit — PCB, display, encoder, GPS
- [ ] 11.4 Iterate on design (usually takes 2-3 revisions)
- [ ] 11.5 Final version in black or dark gray filament
- [ ] 11.6 (Future) Evaluate injection molding if volume justifies it

## Phase 12 — Sponsor Video (PCBWay)

The video you're planning with PCBWay.

- [ ] 12.1 Finish filming A-roll (you using the device, driving, reactions)
- [ ] 12.2 Film B-roll (soldering, assembly, PCB close-ups, 3D printing)
- [ ] 12.3 Film PCBWay unboxing (receiving the boards)
- [ ] 12.4 Screen capture: code walkthrough, serial monitor output
- [ ] 12.5 Edit video: intro → problem → solution → build → demo → PCBWay shoutout
- [ ] 12.6 Add PCBWay affiliate link and repo link in video description
- [ ] 12.7 Upload to YouTube
- [ ] 12.8 Share with PCBWay contact for approval

## Phase 13 — Product Listing Prep

Everything needed before you can sell units.

- [ ] 13.1 Calculate BOM cost per unit (ESP32 + GPS + display + PCB + case + cables)
- [ ] 13.2 Set retail price (typical hardware markup: 3-4x BOM cost)
- [ ] 13.3 Write product description and feature list
- [ ] 13.4 Take professional product photos (white background, lifestyle/car shots)
- [ ] 13.5 Create a simple user manual / quick start guide (1-page PDF)
- [ ] 13.6 Decide on packaging (small box, foam insert, USB cable included?)
- [ ] 13.7 Get TomTom API key instructions ready for customers (or pre-provision keys)
- [ ] 13.8 Decide: do customers need their own API key, or do you provide one?
    - Own key = free for you, slight friction for customer
    - Shared key = smoother UX, but you pay for API overage at scale
    - Recommend: start with customer's own key, evaluate shared key later

## Phase 14 — Online Store

Where to sell it.

### Option A: Etsy Store
- [ ] 14.1 Create Etsy seller account (if you don't have one)
- [ ] 14.2 Create product listing with photos, description, pricing
- [ ] 14.3 Set up shipping profiles (weight, dimensions, methods)
- [ ] 14.4 Etsy fees: 6.5% transaction fee + $0.20 listing fee + payment processing
- [ ] 14.5 Launch listing

### Option B: Leemerie3D Website
- [ ] 14.6 Create product page on leemerie3d
- [ ] 14.7 Set up payment processing (Stripe, PayPal, or Shopify)
- [ ] 14.8 Set up shipping workflow
- [ ] 14.9 Launch product page

### Option C: Both (Recommended)
- [ ] 14.10 List on Etsy for discoverability (people search Etsy for gadgets)
- [ ] 14.11 List on your own site for higher margins (no Etsy fees)
- [ ] 14.12 Link between them (Etsy listing → your site for support/docs)

## Phase 15 — Post-Launch

After you start selling.

- [ ] 15.1 Set up a simple support system (email or Discord server)
- [ ] 15.2 Create a FAQ page on your website or GitHub wiki
- [ ] 15.3 Collect customer feedback
- [ ] 15.4 Plan v2 features based on feedback:
    - Bluetooth phone app for configuration?
    - Speed camera / red light camera warnings?
    - Trip logging / speed history?
    - Integration with OBD-II for actual vehicle speed (more accurate than GPS)?
    - Heads-up display (HUD) mode — reflect off windshield?
- [ ] 15.5 Consider FCC/regulatory requirements if selling in quantity
    - Unintentional radiator (Part 15) — most ESP32 modules are pre-certified
    - If using a pre-certified ESP32 module, you likely just need FCC ID on the label
    - Consult a compliance advisor if selling more than ~100 units

---

## Cost Estimate (Per Unit, Approximate)

| Component | Estimated Cost |
|-----------|---------------|
| ESP32-S3 WROOM module | $3.00 |
| GPS module (BN-220) | $8.00 |
| 2.0" IPS TFT display | $5.00 |
| Custom PCB (at 100 qty) | $1.50 |
| Rotary encoder | $0.75 |
| USB-C connector + LDO + passives | $1.50 |
| 3D printed case | $2.00 |
| USB-C cable (included) | $1.50 |
| Packaging | $1.00 |
| **Total BOM** | **~$24.25** |

At 3x markup: **~$75 retail price**
At 4x markup: **~$97 retail price**

A $79.99 or $89.99 price point feels right for a niche automotive gadget.

---

## Timeline (Rough Estimate)

| Phase | When |
|-------|------|
| Phases 1-2 | Done |
| Phase 3 (testing) | This week |
| Phases 4-6 (features) | 2-4 weeks |
| Phases 7-9 (WiFi setup, OTA) | 2-3 weeks |
| Phase 10 (PCB design) | 2-4 weeks (depends on PCBWay turnaround) |
| Phase 11 (enclosure) | 1-2 weeks (parallel with PCB) |
| Phase 12 (video) | 1 week |
| Phases 13-14 (listing) | 1 week |

**Total: roughly 2-3 months from now to first sale**, depending on how much time you put in per week.
