# DRC Fixes Checklist — Speed Limit Display PCB

Print this out and check off each item as you complete it.
Run DRC again when all items are done.

---

## 1. Fix Missing Connections

These pins SHOULD be connected but aren't.

| Done | Component | Pin | Connect To |
|------|-----------|-----|-----------|
| [ ] | U3 Display | Pin 1 (GND) | GND symbol |
| [ ] | U3 Display | Pin 4 (VDD) | +3V3 label |
| [ ] | U3 Display | Pin 5 (GND) | GND symbol |
| [ ] | U3 Display | Pin 6 (GND) | GND symbol |
| [ ] | BootButton | Pin 1 | ESP32 IO0 (net label) |
| [ ] | ESP32 (U1) | Pin 41 (EP) | GND symbol |
| [ ] | AMS1117 (U2) | Pin 4 (VOUT) | +3V3 label |
| [ ] | Encoder (U6) | Pin 2 (GND) | GND symbol |

---

## 2. Fix Resistor Footprints

R5 and R6 are missing PCB footprints.

| Done | Component | How to Fix |
|------|-----------|-----------|
| [ ] | R5 (5.1K) | Click R5 → Properties → Footprint → search "0402" → select standard 0402 |
| [ ] | R6 (5.1K) | Click R6 → Properties → Footprint → search "0402" → select standard 0402 |

---

## 3. Wire Button Extra Pins

4-pin pushbuttons have duplicate pins. Wire the extras to match.

| Done | Component | Pin 3 → | Pin 4 → |
|------|-----------|---------|---------|
| [ ] | ResetButton | Same net as Pin 1 (ESP32 EN) | Same net as Pin 2 (GND) |
| [ ] | BootButton | Same net as Pin 1 (ESP32 IO0) | Same net as Pin 2 (GND) |

---

## 4. Place No Connect Flags

These pins are intentionally unused. Place a No Connect flag on each one.
Go to: Place > No Connect Flag(C)

### ESP32 (U1) — unused GPIOs
| Done | Pin |
|------|-----|
| [ ] | IO4 (pin 4) |
| [ ] | IO15 (pin 9) |
| [ ] | IO8 (pin 12) |
| [ ] | IO3 (pin 15) |
| [ ] | IO46 (pin 16) |
| [ ] | IO12 (pin 23) |
| [ ] | IO13 (pin 24) |
| [ ] | IO14 (pin 25) |
| [ ] | IO21 (pin 26) |
| [ ] | IO36 (pin 28) |
| [ ] | IO35 (pin 29) |
| [ ] | IO37 (pin 30) |
| [ ] | IO38 (pin 31) |
| [ ] | IO39 (pin 32) |
| [ ] | IO40 (pin 33) |
| [ ] | IO41 (pin 34) |
| [ ] | IO42 (pin 35) |
| [ ] | RXD0 (pin 36) |
| [ ] | TXD0 (pin 37) |

### GPS ATGM336H — unused pins
| Done | Pin |
|------|-----|
| [ ] | Pin 4 (ON/OFF) |
| [ ] | Pin 5 |
| [ ] | Pin 6 (VBAT) |
| [ ] | Pin 9 (NRST) |
| [ ] | Pin 14 (VCC_RF) |
| [ ] | Pin 16 (SDA) |
| [ ] | Pin 17 (SCL) |

---

## 5. Ignore (Not a Problem)

| Item | Why It's OK |
|------|------------|
| USB-C EP pad mismatch | Shell gets grounded through GND pins already |
| "Component attributes" warning | Informational only, does not affect circuit |

---

## 6. Final Verification

| Done | Step |
|------|------|
| [ ] | Run DRC again (Design > Design Rule Check) |
| [ ] | Target: 0 Fatal Errors, 0 Errors |
| [ ] | Remaining warnings should only be about the USB-C EP pad |
| [ ] | Click on each net to verify it highlights correctly |

---

## Quick Reference — EasyEDA Pro Shortcuts

| Action | How |
|--------|-----|
| Wire | Alt+W |
| Net Label | Alt+N |
| No Connect Flag | Place > No Connect Flag(C) |
| Select/Pointer | Press Escape |
| Zoom | Pinch trackpad or Ctrl+scroll |
| Undo | Ctrl+Z |
| Delete | Select + Delete key |
| Rotate while placing | R |
| Flip while placing | X or Y |
| GND symbol | Wiring Tools toolbar (triangle icon) |
| Run DRC | Design > Design Rule Check |

---

*Print date: ___________*
*All items complete: [ ] YES*
