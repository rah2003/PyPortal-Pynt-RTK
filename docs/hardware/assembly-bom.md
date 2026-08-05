# Final assembly — full parts list (rev B "D" enclosure)

Status 2026-07-28: full shell + band print confirmed good on the MK4S in
PETG (organic supports). ANT_BCD resolved from the Calian drawing:
**Ø26.6** — the first full shell printed with the 28.0 placeholder, so
its 3 antenna holes sit 0.7 mm out (salvage: elongate ~0.5 inward, or
reprint from the corrected `b_shell_full.stl`).

## Printed parts (PETG, nut-side down)

| # | Qty | Part | Source |
|---|---|---|---|
| P1 | 1 | Shell D — main enclosure (integral lid, D-funnel, nut boss) | `enclosure/prints/b_shell_full.stl` |
| P2 | 1 | Case back D — curved band with carrier bosses | `enclosure/prints/b_band_full.stl` |

## Electronics

| # | Qty | Part | Notes |
|---|---|---|---|
| E1 | 1 | Adafruit PyPortal Pynt | slides into the ear slots, screen at the front window |
| E2 | 1 | ArduSimple simpleRTK2B **Lite** (ZED-F9P) | plugs into the carrier's XBee headers |
| E3 | 1 | ArduSimple XBee-to-USB carrier (**mini-USB** FTDI) | mechanical mount for the Lite, screwed to the band; USB is internal/service-only (one-driver rule — never together with the JST link) |
| E4 | 1 | HC977 helical GNSS antenna | on the lid, full Ø46 base supported; north hole aims rearward |

## RF chain

| # | Qty | Part | Notes |
|---|---|---|---|
| R1 | 1 | SMA female bulkhead adapter, plain round thread | through the lid hole; anti-rotation via lock washer (no D-flat) |
| R2 | 1 | bulkhead nut + lock washer | comes with R1 |
| R3 | 1 | SMA extension cable | bulkhead (inside) → Lite's SMA jack |

## Fasteners & inserts

| # | Qty | Part | Where |
|---|---|---|---|
| F1 | 6 | M3×4 heat-set insert (Ø4.0 pilots) | 2 in the band's floor-screw bosses (set from the undersides, **before** the band goes in), 4 in the carrier bosses |
| F2 | 2 | M3×8 pan-head | up through the funnel counterbores into F1 — clamps the band to the floor |
| F3 | 4 | M3×6 pan-head | carrier board → carrier boss inserts |
| F4 | 2 | M3×6 pan-head | Pynt top ears → direct-thread into the Ø2.5 pilots (no inserts — no room beside the glass) |
| F5 | 3 | M2.5×8 | through the lid into the HC977's integral inserts — BCD **Ø26.6**, 6 mm deep, 120° (Calian drawing); ~4.7 mm engagement |
| F6 | 1 | 5/8"-11 hex nut (survey standard) | dropped into the boss pocket before pole mounting; captured, not glued |

## Cables & power

| # | Qty | Part | Notes |
|---|---|---|---|
| C1 | 1 | Pixhawk JST-GH ↔ D3/D4 harness (built, tape-labeled) | 4 conductors: Lite pin 1/6 → D4 VCC/GND (5 V power), Lite pin 3 (TX) → D3 signal, D4 signal → Lite pin 2 (RX) — see `wiring.md` |
| C2 | 1 | USB cable, micro-B | power bank → Pynt micro-USB, through the bottom USB slot |
| C3 | 1 | USB battery bank, 1 port ≥1 A | external — carried, not enclosed (future dock is a separate design) |

## Future / not installed

| Part | Why listed |
|---|---|
| XBee radio in the Lite's top socket | UART2 + the socket are reserved for it; the enclosure's radio void already clears the stack |
| mini-USB cable for the carrier | service/u-center config only, case open, JST link disconnected first |

## Assembly order

1. Set all 6 heat-set inserts (F1) into the printed parts.
2. Drop the 5/8"-11 nut (F6) into the boss pocket.
3. Bulkhead (R1/R2) into the lid, lock washer inside; connect the SMA
   extension (R3) and leave it dangling.
4. Slide the Pynt (E1) down the ear slots; **start both top screws
   (F4) before tightening either** — the board centers on the screws
   (the slots no longer end-stop it laterally).
5. Connect the D3/D4 harness (C1) to the Pynt.
6. Screw the carrier (E3) to the band bosses (F3); seat the Lite (E2);
   connect its JST end of C1 and the SMA extension (R3).
7. Band in: top edge up into the lid groove, tongues into the side
   grooves (anti-flex ledges back them), drop onto the floor; 2× M3×8
   (F2) up through the funnel.
8. Antenna (E4) on the lid pad, north rearward, 3× M2.5×8 (F5).
9. Route C2 through the USB slot to the power bank.
