# Speed Limit Display — Full Product Plan

From prototype to product listing. Check off each task as you complete it.

---

## Phase 3: PCB Design (DONE)

| Done | # | Task | Details |
|------|---|------|---------|
| [x] | 3.1 | Fix DRC errors in schematic | All schematic errors resolved |
| [x] | 3.2 | Convert schematic to PCB layout | Full layout in KiCad 10.0 |
| [x] | 3.3 | Route PCB traces + ground plane | All traces routed, GND pour on F.Cu + B.Cu, stitching vias |
| [x] | 3.4 | Add breakout header (J4) | 9-pin header: IO35, IO36, IO45, IO46, TXD0, RXD0, +3V3, +5V, GND |
| [x] | 3.5 | Export Gerbers | Gerber + drill files generated, zipped and ready to upload |
| [ ] | 3.6 | Order from PCBWay/JLCPCB | Upload Gerber zip, order 5-10 boards |

---

## Phase 4: Testing & Bug Fixes

| Done | # | Task | Details |
|------|---|------|---------|
| [ ] | 4.1 | Road test current code | Suburban, highway, rural. Log serial output. |
| [ ] | 4.2 | Fix bugs from road testing | Tune config values, fix any issues found |

---

## Phase 5: New Features

| Done | # | Task | Details |
|------|---|------|---------|
| [ ] | 5.1 | Buzzer overspeed alert | Beep on yellow/red, configurable in menu |
| [ ] | 5.2 | Road name on display | Parse from TomTom API, show in small text |
| [ ] | 5.3 | Speed limit change animation | Brief flash when limit changes |

---

## Phase 6: Product-Ready Software

| Done | # | Task | Details |
|------|---|------|---------|
| [ ] | 6.1 | WiFi setup portal | First-boot hotspot + web config page. REQUIRED for selling. |
| [ ] | 6.2 | OTA firmware updates | Update over WiFi, no USB needed |

---

## Phase 7: Hardware Assembly

| Done | # | Task | Details |
|------|---|------|---------|
| [ ] | 7.1 | 3D printed enclosure | Design in Fusion 360, print, test fit |
| [ ] | 7.2 | First complete prototype | Solder PCB + flash firmware + assemble in case |

---

## Phase 8: Marketing Content

| Done | # | Task | Details |
|------|---|------|---------|
| [ ] | 8.1 | PCBWay sponsor video | Film, edit, upload to YouTube |

---

## Phase 9: Product Prep

| Done | # | Task | Details |
|------|---|------|---------|
| [ ] | 9.1 | Calculate BOM cost + set price | Target $79-89 retail at 3-4x markup |
| [ ] | 9.2 | Product photos | White background + car lifestyle shots |
| [ ] | 9.3 | Product description + quick start guide | For Etsy and website listings |
| [ ] | 9.4 | Packaging design | Box, cable, guide, mount |

---

## Phase 10: Store Setup

| Done | # | Task | Details |
|------|---|------|---------|
| [ ] | 10.1 | Etsy listing | Create seller account, list product |
| [ ] | 10.2 | WordPress product page | Add to leemerie3d site with WooCommerce |
| [ ] | 10.3 | Customer support system | Email or Discord + FAQ page |

---

## Phase 11: Beta Testing

| Done | # | Task | Details |
|------|---|------|---------|
| [ ] | 11.1 | Beta test with 5-10 people | Build units, collect feedback |
| [ ] | 11.2 | Apply feedback + finalize v1.0 | Fix issues, tag release on GitHub |

---

## Phase 12: Launch

| Done | # | Task | Details |
|------|---|------|---------|
| [ ] | 12.1 | Launch on Etsy + website | Go live! Share on social media. |
| [ ] | 12.2 | Post-launch + v2 planning | Monitor 30 days, plan next version |

---

## Progress Tracker

| Phase | Tasks | Status |
|-------|-------|--------|
| 1-2 Code Foundation | All | DONE |
| 3 PCB Design | 5/6 | Nearly Done (order boards) |
| 4 Testing | 0/2 | Not Started |
| 5 Features | 0/3 | Not Started |
| 6 Product Software | 0/2 | Not Started |
| 7 Hardware Assembly | 0/2 | Not Started |
| 8 Marketing | 0/1 | Not Started |
| 9 Product Prep | 0/4 | Not Started |
| 10 Store Setup | 0/3 | Not Started |
| 11 Beta Testing | 0/2 | Not Started |
| 12 Launch | 0/2 | Not Started |
| **Total** | **0/25** | |

---

## What Can Happen In Parallel

Some tasks don't depend on each other and can happen at the same time:

- **PCB design** (Phase 3) + **Road testing** (Phase 4) — test code while designing board
- **New features** (Phase 5) + **PCB manufacturing** (waiting for boards) — code while PCBWay makes boards
- **Enclosure design** (Phase 7.1) + **PCB assembly** (Phase 7.2) — design case while boards ship
- **Video filming** (Phase 8) — film throughout the whole process, not just at the end
- **Product photos** (Phase 9.2) + **Store setup** (Phase 10) — set up accounts while waiting for final photos

---

*Print date: ___________*
