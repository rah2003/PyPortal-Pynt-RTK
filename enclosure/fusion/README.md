# Fusion workflow

`pynt_rtk_enclosure.py` builds the rev A enclosure (Shell + Cover
components) as a fresh parametric document inside Autodesk Fusion. It is
a geometry port of `../pynt_rtk_enclosure.scad`; design rationale lives
in `docs/hardware/enclosure.md`. Edit the constants at the top of the
script and re-run to regenerate — it never touches the active document.

## Run it

1. Fusion → **Utilities → Add-Ins → Scripts and Add-Ins** (Shift+S)
2. Green **+** next to *My Scripts*, browse to this `enclosure/fusion/`
   folder
3. Select `pynt_rtk_enclosure` → **Run**

A dialog reports the key derived numbers (exterior size, pole-shoulder →
antenna-pad height for the SW Maps antenna-height entry).

## Fit-check with vendor CAD

`../vendor/` holds the manufacturers' own models — insert them with
**Insert → Insert CAD** and position against the enclosure:

| File | What | Source |
|---|---|---|
| `pyportal_pynt_4465.step` | PyPortal Pynt | Adafruit_CAD_Parts (GitHub) |
| `simpleRTK2B_Lite-00.step` | simpleRTK2B Lite | ardusimple.com |
| `ardusimple_usbc_carrier.step` | USB-C XBee carrier | ardusimple.com |
| `pyportal_pynt_4465.stl` | Pynt mesh (backup) | Adafruit_CAD_Parts |

The folder is gitignored (large binaries); re-download from the sources
above if missing.

## Test prints

Set `PART = 'base_test'` or `'bezel_test'` at the top of the script and
run — the trimmed body is exported automatically to
`enclosure/prints/<part>.stl`. `base_test` is the bottom 25 mm (nut
pocket fit, USB plug vs boss clearance, SD reach, vent grilles);
`bezel_test` is one top bezel corner (window chamfer vs touch reach,
eyebrow, one top-ear insert pocket). Same regions as the .scad `part`
selector. Print both front-face-down like the shell; `base_test` needs
a local support block under the nut boss's bed-facing face (a plain
90° side overhang — see the print notes in `docs/hardware/enclosure.md`
for why it can't be chamfered), `bezel_test` is support-free.

## Export for printing

Right-click **Shell** / **Cover** in the browser → *Save As Mesh* → STL,
or 3D Print. Print notes (material, orientation, the two test prints)
are in `docs/hardware/enclosure.md`.

## Before printing for real

- Carrier dimensions are settled: the owner's mini-USB FTDI carrier is
  identical to the USB-C carrier except the connector (confirmed
  2026-07-19), so the STEP-derived `CARRIER_*` defaults apply.
- `PYNT_MIRROR = -1` confirmed 2026-07-19: USB on the viewer's left
  looking at the screen.
- Cover screws are plain through-holes — use **pan-head** M3s (no
  countersinks modeled).
