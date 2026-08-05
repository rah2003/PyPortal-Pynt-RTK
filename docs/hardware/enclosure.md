# Enclosure — Design Doc (rev A8, 2026-07-20)

3D-printed handheld/pole-top enclosure for the full stack: PyPortal Pynt
(portrait, rotation 2), simpleRTK2B Lite on its USB carrier board, HC977
helical antenna on the lid, 5/8"-11 survey-pole nut captured in the base.
CAD sources: `enclosure/pynt_rtk_enclosure.scad` (parametric OpenSCAD)
and `enclosure/fusion/pynt_rtk_enclosure.py` (Autodesk Fusion build
script, same geometry — see `enclosure/fusion/README.md`). **The
parameter blocks are the single source of truth for numbers** — keep the
two in sync; this doc explains the *why*. Vendor STEP models for
fit-checks live in `enclosure/vendor/`.

Owner decisions (2026-07-18): printed on owner's FDM printer; handheld
unit with internal survey-pole nut; antenna on top of enclosure via the
owner's SMA extension cable; battery bank external for now (bottom
attachment possible later); light-rain resistant, not sealed.

Rev A1 (2026-07-19, after first Fusion build): interior deepened
41.5 → 52 so the full Ø46 antenna pad (HC977 base Ø44.2) lands on the
lid — rev A truncated it at the back edge. SMA bulkhead hole is plain
round (owner's adapter has no D-flat; lock washer inside for
anti-rotation). Battery provisions moved from the front floor corners
to the rear floor behind the nut prism — at the front they collided
with the Pynt's lower mounting ears.

Rev A2 (2026-07-19, owner assembly critique): `pynt_mirror` confirmed
**−1** — USB lands on the viewer's **left** looking at the screen in
rotation-2 portrait. The Pynt's **bottom ears are now pinned, not
screwed** (Ø2.7 pins into the Ø3.0 ear holes): the bottom cover posts
sit ~1 mm off that screw axis, so neither a driver nor an insert iron
could ever reach; the top two ears keep M2.5 heat-set inserts, which
have straight access past the top posts (14 mm off-axis). Vent slots
became 3-finger **grilles** so they can't be mistaken for ports, and
the SD slot's front edge is aligned with the USB slot's.

Rev A3 (2026-07-19): all printed heat-set inserts standardized to
**M3, Ø4.0 pilot** (owner stocks 4/6/8 mm lengths; per-location
lengths in the BOM) — M3 screws are a close fit through the Pynt's
Ø3.0 ear holes. Bottom-battery provisions **removed**; a future
battery dock will be its own design. Added **3× M2.5 through-holes in
the lid** on the HC977's insert bolt circle for a positive antenna
hold-down, one hole aimed dead rearward as the antenna-north
reference (see 8).

Rev A4 (2026-07-19, first test-print findings): bezel_test PASSED on
screen-flush fit and boss height; base_test failed on the locating
pins (horizontal stubs print badly, won't enter the ear holes) and
the round Ø8 top bosses hit the glass side edge (owner measured
~2.5 mm hole-center→glass). Fixes: top bosses are now rectangular,
trimmed 2.2 mm inboard, wall-merged outboard, 45°-tapered bottoms,
**direct-thread M3 (Ø2.5 pilots, no inserts)**; the bottom edge drops
into a **capture channel** beside the SD body sized for the bottom
edge's 2.9 mm display stack (1.0 less than the glass — owner
measured). PCB plane unchanged → USB/SD slots unchanged. Reprint both
test parts.

Rev A5 (2026-07-19): the eyebrow's 45° underside is now **colinear
with the window's top chamfer** — one continuous plane from the
window's inner top edge to the eyebrow tip. The old shape sat ~1 mm
off the chamfer plane and printed as a visible step / double edge
(bezel_test finding).

Rev A6 (2026-07-19): bottom capture moved from the board's bottom edge
to the **ears** — two slots at the side walls with a 2.0 mm gap for
the bare 1.6 mm PCB tabs (0.2 clearance per side) and a 45° rear
lead-in. Assembly is now: **slide the board straight down** until the
ears seat, then drive the two top screws. The rev-A4 bottom-edge
channel beside the SD body is deleted (its 2.9 mm bottom-stack
measurement stays recorded in the component table). Same day: rear
wall reworked — vertical capture face to y 5.7 with the lead-in bevel
halved (owner request: give the board's rear real vertical backing),
outboard gap end filled for print support.

Rev A7 (2026-07-20, base_test fit findings): **zero board standoff**
— the Pynt's bottom edge now sits ON the interior floor (`pynt_y0 =
floor_t`; any gap made USB/SD unreachable — everything keyed to the
board moves down with it, so the frame stays correct). **Bottom wall
halved to 1.4** (`floor_t`) for plug/card reach — combined the
connectors sit ~2.9 closer to the outside face. **SD slot widened
2 mm toward the enclosure center only** (slot read ~2 mm off
outboard). **Teardrop profiles** on all horizontally-printed round
holes (pole, SMA, 3 antenna holes — apex up in the front-face-down
print) so their tops print uncompressed. Ear slots re-referenced to
the floor; the base fill under the ear is gone (no gap remains).
Caveat: the nut load path now crosses a 1.4 floor — verify on the
base_test reprint before trusting pole mounting.

Rev A8 (2026-07-20, owner decision): the REFERENCE print orientation
for **both enclosures is now NUT-SIDE DOWN** (bottom-up). The nut
boss prints flat on the bed (no more support patch under it — that
whole problem disappears); pole / SMA / antenna holes are vertical
now, so they reverted to plain round (a teardrop notch would spoil
the SMA gasket seat); the **teardrops moved to the z-axis holes**
(light pinhole, Pynt pilots, cover-post pilots), which print
horizontally in this orientation. Supports still needed in rev A
bottom-up: the shell's interior lid ceiling (a large flat span) and
the window's top edge (long bridge). The cover still prints flat.

## Rev B — the "D" shell (owner-specified, 2026-07-20)

`enclosure/fusion/pynt_rtk_enclosure_b.py` (Fusion-only; rev A kept).
Owner's markup, implemented literally: **plan view is a "D"** — the
front/Pynt half is identical to rev A (all vertical walls stay
vertical, zero edits to the front), and the square rear corners are
removed. From the **antenna centerline (z 31)** the full-width sides
transition via straight **tangent lines** to a rear **arc R26.8 about
the pole axis** (= the current antenna→body rear standoff, so the
rear-most point stays at z 57.8 and the antenna footprint is always
covered).

- **Case back** = a curved band, same 2.8 thickness, constant profile
  down the entire height, flush with the outer surface. Captured on
  all four edges: it stands on the shell floor; its top edge rides in
  a 1.0 groove under the integral lid; its vertical end edges carry
  **half-thickness tongues (R24–25.4, 3.6° span)** that slide into
  matching grooves rebated into the tangent walls — full-height
  lateral support both sides (first band test read loose — groove
  clearances **halved** to 0.075 radial per side, +0.2/+0.35° angular
  padding; **re-print confirmed a good fit**, 2026-07-25). Behind each
  band end a **1.0 anti-flex ledge** stands proud of the interior face
  (same 0.075 stop gap) so the band bears on it instead of deflecting
  inward; the rib hooks ~0.6 into the tangent wall past the groove end
  so it's braced full-height, and prints support-free nut-side down; and 2× M3×8 come up from BELOW through
  the floor (heads face down — rain-safe) into **M3×4 heat-set
  inserts** (Ø4.0 pilots) in bosses welded to the band — inserts set
  from the boss undersides before the band goes in. Pull the band
  straight back for full-width service access.
- **Carrier**: mount plane z 48.5 (assembly finding 2026-07-26: +1.0
  more standoff needed; gusset undersides steepen to ~51°, still
  support-free) — originally moved inside so
  the M3×4 teardrop pockets keep ≥1.0 outer skin (shallower bosses
  let the cuts show through the back — owner catch), and the grid is
  **raised 2.5** (`carrier_y0` 8.5) so the lower bosses clear the
  case-back insert bosses (interference — owner catch). Every boss
  carries a **45° gusset fin** tapering to the band's inner face, so
  the horizontal bosses print support-free. Stack front z 34.2; radio
  void intact.
- The old rear corner posts are gone with the corners. The **nut boss
  is a mini-D echoing the shell**: flat front at z 16.5 (USB plug
  clearance preserved), straight sides to the pole axis, rounded back
  R16.5 — same 2.4–2.5 walls around the hex pocket as the old prism.
  A **D-shaped funnel skirt** lofts from the boss bottom out to the
  housing outline at the base plane: planar tapers on the flats
  (~51°), lofted arc taper on the back (~59°), shared flat front
  plane at z 16.5 so the USB/SD/vent strip stays clear — and the
  underside becomes self-supporting in the bottom-up print. The
  case-back screws pass straight through the funnel as **plain
  clearance holes with Ø6.6 counterbores** (no added bosses — owner
  decision): at z 48.8 the head seat at y −2 is a complete ring, and
  the bore columns are cut clear down to **below the funnel surface's
  deepest point** (a shallower cut leaves an uncut crescent shelf
  inside the bore that blocks the screw head — owner caught it as a
  non-round mouth). Verified: Ø5.7–6.0 pan head travels the clean
  Ø6.6 column to the seat and back with ≥0.3 radial clearance.
  M3×8 reaches ~4 mm into the inserts. **Sharp exterior edges are
  chamfered**: 1.2 on the front vertical corners and top rim, 0.6 on
  the front strip's 90° edges (bottom line, strip sides, strip↔funnel
  crease at z 16.5 — owner markup); the shallow funnel/wall creases
  are excluded (any chamfer there fails to compute, and at ~40° they
  aren't truly sharp). Test-fit prints:
  `enclosure/prints/b_base_test.stl` (bottom 30 — funnel, boss,
  counterbores, ear slots, USB/SD, tongue grooves) and
  `b_band_test.stl` (band segment with insert bosses + tongues).
  All bottom penetrations and the whole front half carry over from
  rev A8 unchanged; lid pad/SMA/BCD holes unchanged on the pole axis.
- **Print**: both parts nut-side down. The band is a constant-section
  tube segment — zero support; the shell keeps rev A's caveats
  (interior lid ceiling + window top edge). Full print files:
  `enclosure/prints/b_shell_full.stl` (57.8 cm³) and
  `b_band_full.stl` (14.4 cm³), pre-oriented Z-up for the slicer;
  MK4S/PETG settings incl. the lid-ceiling support plan in
  `enclosure/prints/print-settings-mk4s-petg.md` (paint-only
  enforcers: interior ceiling, front strip underside, window lintel;
  0.25 contact Z, snug style, extract the block through the rear
  aperture).

## Concept

```
            HC977 helical (Ø44 base, 62 mm tall)
                 │  screws onto ↓
        ┌── SMA bulkhead jack through lid, on the pole axis ──┐
  ┌─────┴──────────────────────────────┐  ← lid/top face      │
  │  JST headroom (D3/D4 harness U-turn)│                      │
  │ ┌───────────┐  ┌──────────────────┐│                      │
  │ │ PyPortal  │  │ carrier + Lite   ││  ← removable back    │
  │ │ Pynt      │  │ stack on the     ││    cover (4× M3)     │
  │ │ portrait, │  │ back cover;      ││                      │
  │ │ screen in │  │ XBee radio void  ││                      │
  │ │ front face│  │ above the Lite   ││                      │
  │ └───────────┘  └──────────────────┘│                      │
  │  USB out ↓   SD ↓   [5/8-11 nut]───┼── pole axis ─────────┘
  └─────────────────┬──────────────────┘
                    └─ external hex boss under the base, nut captured
                       inside; survey pole threads in from below
```

- **Pole axis** (X = width center, 31 mm behind the front face) carries
  both the 5/8-11 nut below and the SMA bulkhead above, so the antenna
  sits plumb over the pole. It lands behind the box's geometric depth
  center because the bottom boss must clear the USB/SD exits at the
  front of the base — slightly nose-heavy in hand, irrelevant on a pole.
- **All signal wiring is internal.** Only four penetrations exist: SMA
  bulkhead (top, gasket-washer sealed — proper rain-proof joint),
  micro-USB power slot (bottom, faces down), microSD slot (bottom, faces
  down), light-sensor pinhole (front bezel). No side penetrations.

## Component facts the geometry is built on

All verified from vendor CAD unless flagged. Sources: Adafruit
`Adafruit PyPortal Pynt.brd` + `4465 PyPortal Pynt.step`
(github.com/adafruit/Adafruit-PyPortal-PCB, Adafruit_CAD_Parts);
ArduSimple `simpleRTK2B_Lite-00.STEP` + USB-C carrier
`AS-ADP-USBC-TO-XBEE-00-R00.step` (ardusimple.com); Calian HC977
datasheet Rev 202203.

| Component | Numbers that matter |
|---|---|
| PyPortal Pynt (#4465) | Body 66.8 × 43.2 × 1.57 mm — **not** the classic-PyPortal outline. Envelope incl. 4 mounting ears 66.8 × 53.2. Holes Ø3.0 on a 61.72 × 47.88 grid (M2.5). Stack: touch overlay +3.90 front / JSTs +4.80 back → 10.27 total. Viewing window 50.25 × 38.6, centered on the body. Micro-USB + microSD on one short edge (down, in rotation 2), D3/D4 JSTs on the other (up). Reset faces **backward**; light sensor looks through the PCB to the **front**. Owner-measured 2026-07-19: display stack at the bottom edge is 2.9 (1.0 less than the 3.9 glass stack); glass side edges ≈2.5 mm from the top ear hole centers. |
| simpleRTK2B Lite | PCB 27.68 × 41.53 × 1.0. Edge-launch SMA jack protrudes 9.5 past the SMA edge. JST-GH tallest top-side part (+4.35). Underside XBee pins protrude **5.97 below** the PCB. 3× Ø2.1 holes (unused here — the carrier is the mount). |
| USB carrier (ArduSimple XBee FTDI carrier, mini-USB variant) | PCB 41.0 × 26.0 × 1.6; 4 corner holes ~Ø3.6 on a **35.0 × 20.0** grid, 3 mm from each edge. Assembled carrier+Lite stack ≈ 15.3 tall incl. underside solder tails; Lite overhangs the non-USB end, SMA tip ≈ 17.2 past the carrier edge. Dimensions taken from the USB-C variant's STEP (AS-ADP-USBC-TO-XBEE-00) — **owner confirmed 2026-07-19 the mini-USB board is identical except the connector itself**, which is internal-only here, so the USB-C numbers apply as-is. |
| HC977 (33-HC977-35) | Radome: base Ø44.2, top Ø38.8, h 62.4, IP69K, 42 g. SMA male on base underside + 3× M2.5 inserts, 6 deep, 120°, on a **Ø26.6 bolt circle** (Calian mechanical drawing, confirmed 2026-07-28; O-ring on the base underside). Phase center above base: 32 mm (L1) / 37 mm (L2). 2.5–16 V bias — a passive SMA extension passes the F9P's 3.3 V untouched. |
| 5/8"-11 hex nut (steel) | Across flats 23.8, across corners ≈ 27.5, thickness ≈ 13.9. Plastic never carries the thread. |

## Layout decisions

1. **Pynt mounting (rev A4)**: the **top two ears screw** — M3×6
   **threading directly into Ø2.5 pilots** (no inserts: the glass side
   edge is only ~2.5 mm from the hole center, so there is no room for
   an insert-sized boss; M3 is a close fit through the Pynt's Ø3.0
   ear holes; driven from the back with the cover off, straight
   access 14 mm clear of the top posts). Bosses are **rectangular,
   trimmed to 2.2 mm inboard** of the hole center (≈1 mm wall beside
   the pilot, ~0.3 mm clear of the glass), merged into the side walls
   outboard, 45° taper on the bottom face (support-free in any print
   orientation). The **bottom ears slide down into capture slots** at
   the side walls: each slot is a 2.0 mm gap (0.2 clearance per side
   of the 1.6 PCB tab) between a front block (z 3–7.2, trimmed 2.0 mm
   inboard of the hole center to clear the glass) and a rear wall
   with a **vertical capture face (z 9.2) up to y 5.7** — real rear
   backing for the PCB's lower 1.4 mm — topped by a **halved 45°
   lead-in** (y 5.7 → 7.3) for the slide-down entry. The outboard end
   of the gap (past the ear tip, ~0.9 mm from the side wall) is
   filled to anchor the rear wall's printed underside; the remaining
   ~1.6 mm underside overhang over the gap prints clean. **End stops
   relieved to ±27.3 (2026-07-26)**: the first assembly bound one top
   screw — the symmetric stops centered the board while the pilots
   follow the STEP's hole grid (midpoint 0.19 off the body
   centerline), and M3 through the Ø3.0 ear holes has ~zero slop, so
   two conflicting datums can't coexist. The slots now only capture
   the ears vertically; the board centers laterally on the screws
   (start both before tightening). On the USB
   side the rear pieces also stop 0.25 mm outboard of the USB slot so
   the plug path stays clear (the front block, z ≤ 7.2, never
   conflicts — the USB slot spans z 8–13); the vent grilles sit at
   centers ±14.5 to clear the slot blocks. Assembly:
   slide the board straight down until the ears seat, then drive the
   top screws. No bottom screws or pins — the
   bottom cover posts sit ~1 mm off the ear screw axis (no driver
   access) and printed pins don't survive printing. Slots + top
   screws fully constrain the board; the PCB plane stays at z 7.4, so
   the USB/SD slots are unchanged. Standoff height 4.4 mm puts the
   touch overlay ~0.5 mm behind the front wall's inner face
   (foam-tape shim closes the gap and keeps rain from wicking around
   the glass edge) — confirmed flush on the bezel_test print.
2. **Carrier + Lite mount on the back cover**, not the shell: the cover
   prints flat with four M3 bosses at the carrier's 35 × 20 grid, so the
   whole GNSS stack comes out with the cover for service. Lite's SMA
   points **up** (toward the lid bulkhead), which puts the carrier's USB
   end **down** — u-center access is: remove cover, unplug the D3/D4
   pigtail (contention rule in `wiring.md`), plug USB. The port is
   deliberately *not* reachable from outside; opening the back puts the
   pigtail in your hand as a physical reminder of the one-driver rule.
3. **XBee radio void**: ≥12 mm is reserved above the Lite's top face for
   a future socketed XBee-format radio (a whip-antenna radio needs
   25–30 mm and would take a reprinted or bulged cover — accepted).
4. **D3/D4 harness** exits the Pynt's top edge into ~14 mm of headroom
   under the lid, U-turns, and drops to the Lite's JST-GH on the back
   cover. Sized for JST-PH plug depth (~7 mm) plus bend.
5. **Reset has no external hole** (rain win). Field reset = USB power
   cycle; bootloader double-tap happens on the bench with the cover
   off. The button faces the service side, reachable with a stick past
   the carrier.
6. **Light sensor pinhole** (Ø2.5) in the bezel's bare strip below the
   window — needed if Phase 4 auto-dim ever ships; plug it with a dab
   of clear epoxy if rain paranoia wins.
7. **Rain strategy**: eyebrow ridge across the front face above the
   screen window — its 45° underside is colinear with the window's
   top chamfer, one continuous plane with no step; window walls
   chamfered outward so the recessed overlay sheds; cover skirt overlaps the shell in a labyrinth; all
   other penetrations face down; two bottom 3-finger vent grilles
   (heat from the AirLift) double as drains — grille fingers so they
   read as vents, not ports.
8. **Antenna seat**: the lid carries a raised Ø46 pad with the SMA
   bulkhead jack (female, gasket washer outside, nut inside) at its
   center; the HC977's base lands on the pad so side loads don't ride
   on the connector alone. The pad is a **full circle** — the interior
   depth is sized so it fits ahead of the back face (axis 31 + r 23 +
   1 margin = 55 shell depth) and the antenna's whole base is
   supported. The bulkhead hole is **plain round** (Ø6.8 printed):
   the owner's SMA adapter has no D-flat, so anti-rotation comes from
   the lock washer under the inside nut. **Antenna hold-down +
   orientation**: 3× Ø3.1 through-holes on the HC977's insert bolt
   circle — **Ø26.6 per the Calian mechanical drawing** (3× M2.5,
   6 deep, 120°; corrected 2026-07-28 from the 28.0 placeholder that
   made it into the first full shell print — those holes sit 0.7 out
   and need ~0.5 elongated inward to salvage) — taking M2.5×8 screws
   from inside the lid into the antenna's own inserts. One hole aims
   dead **rearward** (+z): mount the antenna with its north reference
   at that screw so antenna-north faces opposite the screen. The HC977
   has no published NGS/IGS calibration or formal north-reference
   point — if the radome carries no N mark, designate the insert
   nearest the cable label as the reference and keep it consistent
   between sessions (azimuth PCV error is small but repeatable).
9. **Pole nut**: external hex boss under the base; the nut drops into a
   hex pocket through the interior floor opening (cover off), and a
   Ø17 clearance hole in the boss floor admits the pole stud. Load
   path: pole shoulder → boss floor → bottom wall. The boss's front
   face sits at z = 16.5, behind both the USB plug body (rear ≈ z 14.5
   for a typical overmold) and the SD card path — that clearance is
   what pushed the pole axis to 31 mm (the interior depth is set by
   the antenna pad instead — see 8).
   **Dry-fit your actual USB cable against a `base_test` print before
   committing** (fat overmolds exceed the 2 mm margin; a right-angle
   cable is the fallback).
10. **Battery attachment: removed** (rev A3, owner decision). A future
    battery dock will be a separate design; the nut boss flats and the
    pole thread remain the natural docking features. The rev A1/A2
    provisions (rear-floor bosses + cover lip notches) are deleted.

## Budget (from the .scad defaults — regenerate after any param change)

- Interior: 55.0 W × 83.0 H × 52.0 D
- Exterior shell: 60.6 W × 88.6 H × 57.8 D (plus 17.2 mm nut boss below,
  1.5 mm antenna pad above)
- Depth stack, front→back: 3.0 wall · 0.5 foam · 3.9 glass · 1.6 PCB ·
  4.8 JSTs · ~16 air · 12 radio void ending at the Lite's top face ·
  carrier+Lite stack 15.3 on 3.5 cover bosses · 2.8 cover (the radio
  void sits over the Lite's socket footprint; the JST-GH and SMA poke
  forward of it at the top edge — not a linear sum). Depth is set by
  the antenna pad (see 8), not the electronics stack.
- Any common printer bed fits it standing on the front face or split.

## Antenna height bookkeeping (for SW Maps)

Pole-shoulder plane (boss underside) → lid pad top ≈ 107 mm (read the
exact value from the .scad echo), + bulkhead/antenna-base stack ≈ 2 mm,
+ 32 mm L1 phase-center offset. So **APC ≈ pole length + ~141 mm** —
recompute from the final print + measured bulkhead stack and put the
result in SW Maps' antenna-height field, per the datasheet offsets.

## Print & hardware notes

- **Material: PETG or ASA** (outdoor UV + parked-car heat; PLA will
  creep at the nut boss). 40 %+ infill or 5 perimeters around the nut
  boss and screw bosses; 2.8 mm walls elsewhere.
- Shell prints front-face-down (window bridges are short; the eyebrow's
  45° underside chamfer is modeled, support-free). Cover prints flat,
  bosses up. **The nut boss is a plain prism** — its bed-facing front
  face (~33 × 17 mm at z 16.5) is a true 90° side overhang and needs a
  **local support block** (paint-on support in the slicer). 45° blends
  are geometrically impossible here: an additive wedge forward of the
  boss sits in the USB/SD plug path, and a subtractive chamfer (any
  direction) breaches the hex-pocket walls (2.4 mm). Everything else
  prints support-free. Applies to `base_test` too — same orientation,
  same support patch.
- Hardware BOM — printed inserts are **M3 heat-set, Ø4.0 pilot**
  (owner stock 4/6/8 mm): **M3×4** in the carrier bosses (blind
  pockets — longer inserts would break out), **M3×6 or ×8** in the
  cover posts (9 mm pilots). The Pynt top ears use **no inserts**:
  M3×6 screws thread directly into Ø2.5 pilots (no room for an insert
  beside the glass; the board is rarely removed). Screws: 2× M3×6
  (Pynt top ears), 4× M3×6 (carrier), 4× M3×10 pan (cover). Plus 3×
  M2.5×6 (antenna hold-down, into the HC977's *own* inserts — verify
  insert depth; through-thickness at the holes is 3.3 mm), 1× steel
  5/8"-11 hex nut, 1× SMA male↔female-bulkhead extension (owner has),
  foam tape for the glass perimeter, optional silicone washer under
  the bulkhead.

## Open items before first full print

- [x] ~~Caliper the carrier~~ — resolved 2026-07-19: owner confirmed the
      mini-USB carrier matches the USB-C carrier except the connector,
      so the STEP-derived `carrier_*` defaults stand. (Sanity-check the
      hole grid at the cover dry-fit anyway — it's a 10-second check
      with the screws in hand.)
- [x] ~~Confirm portrait handedness~~ — resolved 2026-07-19: USB is on
      the **viewer's left** looking at the screen (`pynt_mirror = -1`).
- [ ] Measure inserted-SD-card protrusion (Adafruit doesn't model it) →
      finger-recess depth.
- [x] ~~Measure the HC977 insert bolt circle~~ — resolved 2026-07-28
      from the **Calian mechanical drawing: BCD Ø26.6** (3× M2.5 deep
      6, 120°); `ant_bcd` corrected in all three models. The drawing
      does dimension it — the earlier "not in the datasheet" note was
      wrong (only the spec page omits it). Still check the radome for
      a north/index mark at mounting time.
- [ ] Reprint `base_test` (rev A6: ear capture slots, slide-down
      assembly) → ear slot fit + slide feel, nut pocket fit, USB plug
      clearance, SD reach. First prints confirmed USB/SD slot
      alignment.
- [ ] Reprint `bezel_test` (rev A4: trimmed tapered bosses,
      direct-thread Ø2.5 pilots) → boss-vs-glass clearance and M3
      direct-thread feel. First print confirmed screen-flush fit and
      the 4.4 boss height.
- [x] ~~P3/P4 power checks~~ — passed 2026-07-18 (P3 = 4.77 V under
      load, 30-min soak clean; `bringup-log.md` §3). The remaining
      pre-entombment gate is the Phase 2 **1-hour full-stack soak**.

## Antenna metrology (2026-08-04, owner-confirmed)

ARP = the nut-boss underside (print-bed face) — the global lowest
surface of the assembly (mesh-verified, nothing below it). Stack: ARP
−17.2 → HC977 seat +89.1 ⇒ **A = 106.3 mm**; total case height 107.3.
HC977 PCOs from the seat: L1 32 / L2·L5 37 (±3.0). **ARP → L1 PC =
138.3 mm; ARP → L2/L5 PC = 143.3 mm; coaxial with the pole, 0° tilt,
~0 lateral.** Full derivation + caliper-check protocol:
repo `docs/hardware/antenna-metrology.md`.
