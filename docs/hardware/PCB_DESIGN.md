# Custom PCB Design — Speed Limit Display v2

All-in-one board: ESP32-S3 + GPS + Display + Power + Controls.

Designed in KiCad 10.0 for manufacturing by JLCPCB/PCBWay.

---

## Current Status

**Phase: PCB layout complete, DRC passing (warnings only)**

- Schematic: Complete
- PCB Layout: Complete (2-layer, fully routed)
- DRC: Passing (0 errors, cosmetic warnings only)
- Next: Generate Gerbers and order prototype boards

---

## Design Goals

1. Everything on one board (no breakout modules, no jumper wires)
2. Small enough for a car-mount enclosure
3. USB-C for power and programming
4. Display connector (12-pin header)
5. GPS with on-board antenna (L70-R module with ceramic patch)
6. Rotary encoder + push button for tactile control
7. Boot and reset buttons for firmware flashing
8. Piezo buzzer footprint (optional populate)
9. 2-layer PCB (keeps cost low)

---

## Design Tool

**KiCad 10.0** (migrated from EasyEDA due to missing footprints in EasyEDA library).

### KiCad Project Files

```
PCB Design/
  SCH_Speed-Limit-Display_2026-03-29_v45_2026-04-01-11-26/
    Speed-Limit-Display-v2/
      Speed-Limit-Display-v2.kicad_sch    # Schematic
      Speed-Limit-Display-v2.kicad_pcb    # PCB layout
      Speed-Limit-Display-v2.kicad_pro    # Project settings
      Speed-Limit-Display-v2.kicad_dru    # Custom design rules
      Speed-Limit-Display-v2.csv          # BOM export
```

---

## Component Selection (BOM)

### Core Components

| Ref | Component | Part Number | Package | Qty | Notes |
|-----|-----------|-------------|---------|-----|-------|
| U1 | ESP32-S3-WROOM-1-N8 | ESP32-S3-WROOM-1-N8 | Module (40-pin) | 1 | Dual core, 8MB flash, WiFi+BLE, built-in antenna |
| U2 | GPS Receiver | L70-R (Quectel) | LCC | 1 | NMEA output, built-in ceramic patch antenna |
| U3 | 3.3V LDO Regulator | LV75530PDBV | SOT-23-5 | 1 | 5V USB to 3.3V for ESP32 + GPS |
| U4 | Voltage Level Shifter | — | SOT-23-6 | 1 | Signal level translation |

### Display

| Ref | Component | Notes |
|-----|-----------|-------|
| J2 | 12-pin display connector | Conn_01x12_Pin, 0.5mm pitch header |

### Power & Protection

| Ref | Component | Value | Package | Qty | Notes |
|-----|-----------|-------|---------|-----|-------|
| J1 | USB-C Connector | USB 2.0 | SMD 16-pin | 1 | Power + data (ESP32-S3 native USB) |
| F1 | Resettable fuse | 500mA PTC | 0805 | 1 | USB overcurrent protection |
| D1 | LED indicator | — | SMD | 1 | Power/status |
| D2 | TVS diode | D_TVS500mA | SMD | 1 | USB ESD protection |
| C1-C6 | Decoupling capacitors | 100nF - 10uF | 0805 | 6 | Power filtering |

### User Interface

| Ref | Component | Part Number | Package | Qty | Notes |
|-----|-----------|-------------|---------|-----|-------|
| SW1 | Rotary encoder with push | EC11 compatible | Through-hole | 1 | Rotate = navigate, push = select |
| SW3 | Boot button | Tactile push | SMD | 1 | Hold during reset for firmware flash mode |
| SW4 | Reset button | Tactile push | SMD | 1 | Reset/EN control |
| BZ1 | Piezo buzzer | 5V passive | Through-hole | 1 | Optional — don't populate for silent version |

### Passive Components

| Ref | Component | Value | Package | Qty | Notes |
|-----|-----------|-------|---------|-----|-------|
| R1 | LED resistor | 1K | 0805 | 1 | For D1 |
| R2, R3 | USB CC resistors | 5.1K | 0805 | 2 | Tell USB host we want 5V |
| R4 | Pull-up resistor | 10K | 0805 | 1 | Boot mode pull-up |
| R5, R6 | I2C/signal resistors | 5.1K | 0805 | 2 | Signal conditioning |
| R7, R8 | USB data resistors | 22R | 0805 | 2 | USB D+/D- series termination |

### Connectors

| Ref | Component | Notes |
|-----|-----------|-------|
| J1 | USB-C receptacle | Power input + USB data |
| J2 | 12-pin header | Display connector |
| J3 | Coaxial connector | External GPS antenna (optional) |

---

## Schematic — Block Diagram

```
                                    +---------------------+
   USB-C --> 5V --> LV75530 --> 3V3 |                     |
      |            (LDO)           |  ESP32-S3-WROOM-1   |
      |                            |                     |
      |   D+/D- -------------------|  GPIO 19/20 (USB)   |
      |                            |                     |
      |                            |  SPI Bus --> Display |
      |                            |  (via J2 header)    |
      |                            |                     |
      |                            |  UART --> GPS (U2)  |
      |                            |  GPIO 17 (RX)       |
      |                            |  GPIO 18 (TX)       |
      |                            |                     |
      |                            |  Rotary Encoder:    |
      |                            |  GPIO 5  (A)        |
      |                            |  GPIO 6  (B)        |
      |                            |  GPIO 7  (SW)       |
      |                            |                     |
      |                            |  Buzzer: GPIO 14    |
      |                            |  EN <-- Reset Btn   |
      |                            |  IO0 <-- Boot Btn   |
      |                            +---------------------+
```

---

## PCB Layout Details

### Board Specifications

| Parameter | Value |
|-----------|-------|
| Layers | 2 (F.Cu + B.Cu) |
| Copper weight | 1 oz |
| Board thickness | 1.6mm |
| Surface finish | HASL (lead-free) or ENIG |
| Solder mask | Default (green or black) |
| Silkscreen | White |

### Design Rules Applied

| Rule | Value | Notes |
|------|-------|-------|
| Signal trace width | 0.25mm | Default netclass |
| Power trace width | 0.3mm | Power netclass (+3V3, +5V, GND) |
| Clearance | 0.15mm | Both Default and Power netclasses |
| Min drill size | 0.2mm | For ESP32 thermal vias |
| Via size | 0.5mm pad / 0.2mm drill | Standard stitching and signal vias |
| Copper to edge clearance | 0.0mm | Relaxed for edge-mounted connectors (J1, BZ1) |

### Custom DRC Rules (in .kicad_dru)

```
(rule "USB-C connector"
  (condition "A.Reference == 'J1' && B.Reference == 'J1'")
  (constraint clearance (min 0.1mm))
)

(rule "Edge components"
  (condition "A.Type == 'pad' && (A.Reference == 'J1' || A.Reference == 'BZ1' || A.Reference == 'U2')")
  (constraint edge_clearance (min 0mm))
)
```

### Copper Zones

- **F.Cu GND pour** — solid fill, covers entire board, GND net
- **B.Cu GND pour** — solid fill, covers entire board, GND net
- **Pad connections** — Solid (not thermal relief) for better connectivity
- **Stitching vias** — placed throughout to connect F.Cu and B.Cu ground planes
- **Antenna keep-out zone** — no copper under ESP32 antenna area

### Net Classes

| Netclass | Track Width | Via Size | Clearance | Nets |
|----------|------------|----------|-----------|------|
| Default | 0.25mm | 0.5mm | 0.15mm | All signal nets |
| Power | 0.3mm | 0.5mm | 0.15mm | +3V3, +5V, GND |

---

## DRC Status

### Final DRC Results (2026-04-05)

- **Errors: 0**
- **Warnings: 10** (all cosmetic silkscreen issues)
- **Unconnected pads: 0**

### Remaining Warnings (safe to ignore)

| Warning | Count | Why It's OK |
|---------|-------|------------|
| Silkscreen clipped by board edge | 4 | J1 and SW3 labels at board edge — fab clips automatically |
| Silkscreen overlap | 2 | R7/R8 and C3/J3 reference labels overlap — cosmetic only |
| Silkscreen over copper | 3 | R7 and C2 labels over pads — fab clips automatically |
| Duplicate via | 1 | Two vias at same location — harmless, can delete one |

### Ignored DRC Checks

| Check | Reason |
|-------|--------|
| Board edge clearance | USB-C (J1) and buzzer (BZ1) intentionally at board edge |
| Thermal relief incomplete | B.Cu zone uses solid connections; some pads only have 1 spoke which is sufficient |
| Footprint courtyard | Not all footprints have courtyard defined — does not affect manufacturing |

---

## Manufacturing — Generating Gerbers

### From KiCad:

1. **File > Fabrication Outputs > Gerbers (.gbr)**
2. Select output directory
3. Check all layers: F.Cu, B.Cu, F.Mask, B.Mask, F.Silkscreen, B.Silkscreen, Edge.Cuts
4. Click "Generate"
5. **File > Fabrication Outputs > Drill Files (.drl)**
6. Generate drill files in the same directory

### Files to Upload

| File | Layer |
|------|-------|
| F.Cu.gbr | Front copper |
| B.Cu.gbr | Back copper |
| F.Mask.gbr | Front solder mask |
| B.Mask.gbr | Back solder mask |
| F.Silkscreen.gbr | Front silkscreen |
| B.Silkscreen.gbr | Back silkscreen |
| Edge.Cuts.gbr | Board outline |
| *.drl | Drill files |

### Recommended Fab Houses

| Fab House | Notes |
|-----------|-------|
| **JLCPCB** | Cheapest for prototypes, fast turnaround, supports 0.2mm min drill |
| **PCBWay** | Good quality, assembly service available |

### Order Specs

- Layers: 2
- Quantity: 5 (prototype)
- Thickness: 1.6mm
- Surface finish: HASL lead-free
- Solder mask color: Your choice (black looks professional)
- Silkscreen: White
- Min hole size: 0.2mm
- Min trace/space: 0.15mm / 0.15mm

---

## Assembly Notes

### Components Requiring Hand Soldering
- ESP32-S3-WROOM-1 (large module, requires careful alignment)
- USB-C connector (fine pitch pads)
- All SMD passives (0805 size — manageable with soldering iron)

### Assembly Order (recommended)
1. LDO regulator (U3) and decoupling caps (C1-C6)
2. USB-C connector (J1) and protection components (F1, D2)
3. USB resistors (R2, R3, R7, R8)
4. ESP32-S3 module (U1) — solder pads first, then reflow ground pad
5. GPS module (U2) and antenna connector (J3)
6. Signal resistors (R1, R4-R6)
7. Level shifter (U4) and LED (D1)
8. Through-hole components last: rotary encoder (SW1), buzzer (BZ1)
9. Push buttons (SW3, SW4)
10. Display connector (J2)

### Testing After Assembly
1. Check for shorts between +5V, +3V3, and GND with multimeter
2. Apply USB power — verify 3.3V on LDO output
3. Connect via USB — ESP32 should appear as USB device
4. Flash firmware and verify GPS, display, encoder, buttons, buzzer

---

## Future-Proofing: Breakout Header (J4)

A 9-pin breakout header (J4) was added to expose unused GPIO pins and power rails
for future expansion without requiring a board respin.

| J4 Pin | Net | Purpose |
|--------|-----|---------|
| 1 | +3V3 | 3.3V power output |
| 2 | +5V | 5V power (direct from USB) |
| 3 | GND | Ground |
| 4 | IO35 | General purpose GPIO (SPI, buttons) |
| 5 | IO36 | General purpose GPIO (SPI, buttons) |
| 6 | IO45 | General purpose GPIO (I2C for sensors) |
| 7 | IO46 | General purpose GPIO (I2C for sensors) |
| 8 | TXD0 | Debug UART TX (serial monitor without USB) |
| 9 | RXD0 | Debug UART RX (serial monitor without USB) |

**Potential uses:**
- Ambient light sensor (BH1750 or VEML7700 via I2C on IO45/IO46) for auto-brightness
- Accelerometer for motion detection
- External serial debug without occupying USB port
- Additional buttons or sensors
- Powering small external accessories (3.3V or 5V)

---

## Generating Gerber Files

### Step-by-step in KiCad:

#### 1. Generate Gerber files

1. Open the PCB editor (Speed-Limit-Display-v2.kicad_pcb)
2. Go to **File > Fabrication Outputs > Gerbers (.gbr)**
3. Set the **Output directory** to a folder like `Gerbers/`
4. Under **Layers**, check all of these:
   - F.Cu (front copper)
   - B.Cu (back copper)
   - F.Paste (front paste — for stencil)
   - B.Paste (back paste)
   - F.Silkscreen (front silkscreen)
   - B.Silkscreen (back silkscreen)
   - F.Mask (front solder mask)
   - B.Mask (back solder mask)
   - Edge.Cuts (board outline)
5. Under **General Options**:
   - Check "Use Protel filename extensions" (for compatibility)
   - Check "Subtract soldermask from silkscreen"
6. Click **Plot** to generate

#### 2. Generate Drill files

1. In the same Gerber dialog, click **Generate Drill Files...**
2. Set format to **Excellon**
3. Check "PTH and NPTH in single file" (most fabs prefer this)
4. Drill units: **Millimeters**
5. Click **Generate Drill File**

#### 3. Verify Gerbers

1. In KiCad, go to **File > Fabrication Outputs > Gerber Viewer** (or use the standalone GerbView)
2. Open all generated files and visually check:
   - All traces visible on copper layers
   - Board outline is a closed shape
   - Solder mask openings align with pads
   - Silkscreen is readable and not overlapping pads
   - Drill holes are present and correctly sized

### Output files (with Protel extensions)

| File | Description |
|------|-------------|
| .GTL | Front copper (F.Cu) |
| .GBL | Back copper (B.Cu) |
| .GTS | Front solder mask (F.Mask) |
| .GBS | Back solder mask (B.Mask) |
| .GTO | Front silkscreen (F.Silkscreen) |
| .GBO | Back silkscreen (B.Silkscreen) |
| .GTP | Front paste (F.Paste) |
| .GBP | Back paste (B.Paste) |
| .GKO or .GM1 | Board outline (Edge.Cuts) |
| .DRL or .XLN | Drill file |

---

## Ordering from PCBWay

### Step-by-step:

1. **Zip all Gerber + drill files** into a single .zip file
2. Go to [pcbway.com](https://www.pcbway.com) and click **Quote Now**
3. Click **Quick-order PCB** > **Add Gerber File** and upload your .zip
4. PCBWay will auto-detect board dimensions and layer count
5. Set the following specs:

| Setting | Value |
|---------|-------|
| Layers | 2 |
| Dimensions | (auto-detected from Gerbers) |
| Quantity | 5 (minimum for prototype) |
| Thickness | 1.6mm |
| Surface finish | HASL (lead-free) — cheapest option |
| Solder mask color | Black (or your preference) |
| Silkscreen color | White |
| Min hole size | 0.2mm |
| Min track/spacing | 6/6 mil (0.15mm/0.15mm) |
| Copper weight | 1 oz |
| Gold fingers | No |
| Castellated holes | No |

6. Click **Save to Cart** and proceed to checkout
7. Typical turnaround: 3-5 business days manufacturing + shipping

### Ordering from JLCPCB (alternative)

1. Go to [jlcpcb.com](https://www.jlcpcb.com) and click **Order Now**
2. Upload the same .zip file
3. JLCPCB auto-detects specs — review and adjust:
   - Same settings as PCBWay table above
   - JLCPCB offers 5 PCBs for ~$2 + shipping
   - Select shipping method (DHL ~$15-20 for 5-7 day delivery)
4. Add to cart and checkout

### Assembly Service (PCBA) — Optional

Both PCBWay and JLCPCB offer assembly where they solder components for you.
To use this service, you also need:

1. **BOM file** (Bill of Materials) — exported from KiCad:
   - Go to schematic editor > **Tools > Edit Symbol Fields** > **Export to CSV**
   - Or use the existing `Speed-Limit-Display-v2.csv`
   - Format: Reference, Value, Footprint, LCSC Part Number

2. **CPL file** (Component Placement List) — exported from KiCad:
   - Go to PCB editor > **File > Fabrication Outputs > Component Placement (.pos)**
   - This tells the pick-and-place machine where to put each component

3. Upload BOM + CPL along with Gerbers when ordering
4. The fab house will show you which parts they have in stock
5. For parts not in stock, you may need to supply them yourself

**Note:** Assembly adds ~$30-50 for small quantities. For 5 prototype boards,
hand soldering is usually more cost-effective. Use assembly service for
larger batches (20+).

---

## Ordering Components

### Recommended Suppliers

| Supplier | Best for | Notes |
|----------|----------|-------|
| **LCSC** | All SMD parts, ESP32 modules | Cheapest, ships from China, pairs with JLCPCB |
| **Digikey** | Everything | Fast US shipping, best search/filtering |
| **Mouser** | Everything | Similar to Digikey, good stock |
| **AliExpress** | GPS modules, displays, encoders | Cheapest for breakout modules, slow shipping |

### Key Parts to Source

| Component | Where to Buy | Search Term |
|-----------|-------------|-------------|
| ESP32-S3-WROOM-1-N8 | LCSC or Digikey | ESP32-S3-WROOM-1-N8 |
| Quectel L70-R GPS | LCSC or AliExpress | L70-R or L70-RE |
| USB-C connector | LCSC | USB-C 16P SMD |
| EC11 rotary encoder | AliExpress or LCSC | EC11 encoder with switch |
| 0805 resistors/caps | LCSC | Order assortment kit |
| Piezo buzzer | LCSC or AliExpress | 12mm passive buzzer |

---

## Design History

| Date | Change |
|------|--------|
| 2026-03-29 | Initial schematic created in EasyEDA |
| 2026-04-01 | Migrated to KiCad 10.0 (EasyEDA missing footprints) |
| 2026-04-01 | Schematic rebuild in KiCad with all components |
| 2026-04-05 | PCB layout complete — all traces routed |
| 2026-04-05 | GND copper pour added (F.Cu + B.Cu) |
| 2026-04-05 | DRC passing with 0 errors |
| 2026-04-06 | Added J4 breakout header (9-pin) for future expansion |
| 2026-04-06 | Final DRC: 0 errors, 2 cosmetic warnings only |

---

## Next Steps

1. **Generate Gerbers** from KiCad (see instructions above)
2. **Verify Gerbers** in GerbView or online viewer
3. **Upload to PCBWay or JLCPCB** and order prototype boards
4. **Order components** from LCSC or Digikey
5. **Assemble prototype** board (hand solder or PCBA service)
6. **Test and validate** all circuits
7. **Update firmware** config.h pin assignments to match PCB
8. **Iterate** if needed based on testing results
