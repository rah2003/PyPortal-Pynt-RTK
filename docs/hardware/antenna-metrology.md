# Antenna Metrology — ARP & Phase-Center Offsets (2026-08-04)

Closes the 2026-08-03 team review's **P6** finding ("no ARP, no antenna
height, no phase-centre data anywhere"). Owner-confirmed 2026-08-04.

## ARP definition (this build)

**ARP = the nut-boss underside** — the D-shaped ring (679 mm²) around
the Ø17 pole hole at the bottom of the enclosure. It is simultaneously:

- the bottom face of the 5/8"-11 nut pocket's boss,
- the **global lowest surface of the assembled enclosure** (verified by
  exhaustive enumeration of every down-facing face in the as-printed
  meshes `enclosure/prints/b_shell_full.stl` + `b_band_full.stl`:
  zero vertices below it), and
- the **print-bed face** (both rev-B parts print nut-side down).

The pole stud passes through the Ø17 clearance hole; the 5/8"-11 nut is
captured in the hex pocket above; nothing seats internally — the pole
top bears on this same exterior face.

## Enclosure vertical stack (model frame: 0 = housing exterior floor)

| Plane | Height | Source |
|---|---|---|
| ARP (nut-boss face, print bed) | **−17.2** | `BOSS_H = nut 13.9 + 0.3 + floor 3.0` |
| Screw-head seats (recessed in funnel) | −2.0 | mesh |
| Housing exterior bottom (USB/SD face, 756 mm²) | 0.0 | mesh |
| Shell top exterior | +88.6 | `EXT_H` |
| **HC977 seat plane** (Ø44.8 recess floor in the lid pad) | **+89.1** | `EXT_H + pad 1.5 − recess 1.0` |
| Antenna pad top | +90.1 | mesh |

**A = ARP → antenna seat = 89.1 − (−17.2) = 106.3 mm.
Total case height (ARP → pad top) = 107.3 mm.**

Geometry facts (measured from the meshes, not just the source scripts):
pole axis and antenna boresight are **collinear** (hole/recess/SMA
centroids concentric within ≤0.3 mm of tessellation noise), both normal
to the ARP plane — **0° tilt, ~0 horizontal offset**. The axis sits at
X = 0, 31.0 mm behind the front face. Antenna clocking: the a=0 M2.5
hole points dead rearward (antenna "north" away from the screen).

## HC977 phase-center data (Calian datasheet Rev. 202203 + mechanical drawing)

Antenna: Calian/Tallysman **HC977**, part 33-HC977-35 (35 dB LNA),
s/n 20200624 (Q15). PCO along the antenna z-axis from its base
(= our seat plane):

- **L1: 32 mm** · **L2/L5: 37 mm** · PC variation **±3.0 mm** per band
  (note: L1 and L2/L5 are *distinct* nominal phase centers 5 mm apart —
  not one shared PC)
- Sources: datasheet spec table ("PCO (z-axis, mm): 32 (L1), 37 (L2)")
  and `HC977_MECHANICAL_DRAWING.svg` ("L1 = 32 mm, L2 / L5 = 37 mm"),
  both at <https://www.tallysman.com/product/hc977-triple-band-helical-antenna-with-l-band/>
- Antenna: Ø44.2 × 62.4 mm, 42 g, base mount 3× M2.5 (6 mm deep, 120°,
  Ø26.6 BCD) — **no native 5/8"-11 thread**; this enclosure IS the
  5/8"-11 adapter.

## The numbers a surveyor needs

```
ARP → L1  phase center = 106.3 + 32 = 138.3 mm   (±3.0)
ARP → L2/L5 phase center = 106.3 + 37 = 143.3 mm (±3.0)
horizontal PC offset from pole axis ≈ 0 (coaxial, 0° tilt)
```

Antenna height for RTK work = pole/ARP height + the offset above for
the band your software expects (L1 is the usual convention). For RINEX
post-processing of the RAWX logs: ANTENNA: DELTA H = ARP height; apply
the per-band PCOs from this table.

## Verification note (one-time caliper check, still open)

The stack above is **design-nominal from the print meshes**. FDM
first-layer squish and the HC977's base O-ring can move reality a few
tenths of a millimeter. To close: caliper the assembled case —
**ARP face → pad top should read 107.3 mm** (or ARP → recess floor
106.3 with the antenna off). Record the measured value here.

Annotated views (generated from the print meshes):
`pynt-arp-side-annotated.png`, `pynt-arp-top-annotated.png` (this
directory). Design source: `enclosure/` at the repo root.
