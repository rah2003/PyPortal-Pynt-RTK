# Rev B full print — Prusa MK4S / PETG settings

Files (pre-oriented: Z-up = print orientation, both land nut-side /
floor-side down in the slicer — no manual rotation needed):

| File | Part | Volume | Height on bed |
|---|---|---|---|
| `b_shell_full.stl` | Shell D (main enclosure, integral lid + funnel + boss) | 57.8 cm³ | 107.3 mm |
| `b_band_full.stl` | Case back D (curved band + carrier bosses) | 14.4 cm³ | 85.1 mm |

Profiles: **Original Prusa MK4S, 0.4 nozzle, "0.20mm SPEED"** structural
base (drop to 0.15 QUALITY if you want a crisper window edge — adds ~2 h).
Filament: **Prusament PETG defaults** — nozzle 240 °C, bed 85 °C, fan
20–50 %, no cooling first 3 layers.

| Setting | Value |
|---|---|
| Perimeters | 3 (2.8 walls = 3 loops + light infill) |
| Top / bottom layers | 5 / 4 |
| Infill | 15 % gyroid |
| Brim | **5 mm** on the shell (it stands on the ~33 mm boss footprint until the funnel widens — tall part, small first contact); 4 mm on the band |
| Seam | Rear, aligned/painted on the rear arc centerline (hidden against the pole) |
| Supports mode | **"For support enforcers only"** — paint them; do NOT use auto/everywhere |

## Supporting the top of the main enclosure (the lid ceiling)

The integral lid's **interior ceiling** is the one large flat overhang:
a ~55 × 51 mm D-shaped roof at **103 mm above the bed**, spanning the open
cavity. Supports for it tower ~84 mm up from the interior floor, so they
must be allowed to land **inside the part** ("support on build plate
only" = OFF).

Paint an enforcer across the whole interior ceiling, then:

| Support setting | Value | Why |
|---|---|---|
| Style | **Organic** (owner-confirmed on the full print) or Snug | Organic printed the ceiling cleanly on the MK4S; Snug remains the fallback if a future print's ceiling reads ropey |
| Top contact Z distance | **0.25 mm** (go 0.30 if it still welds) | PETG fuses to PETG at the default 0.2 |
| Interface layers | 3 top / 2 bottom | Flat ceiling needs a dense roof to avoid sag between pillars |
| Interface pattern spacing | 0.2 mm | Near-solid interface under the ceiling |
| Support/object XY separation | **0.8 mm** | Keeps pillars from bonding to the interior walls and bosses |
| Interface loops | Off | Easier extraction |
| Pattern spacing | 2.5 mm | Stiff enough at 84 mm tall without becoming a solid brick |

**Extraction path:** print the shell *without* the band installed slot —
the whole rear arc aperture is open, so after printing the support block
tilts and slides out **rearward** through the case-back opening in one
piece. Don't fish it through the window.

Two smaller enforcer zones while you're painting:

- **Front strip underside** (the USB/SD/vent strip, first 16.5 mm behind
  the front face): it's a flat downward face 17 mm above the bed —
  outside the funnel's footprint, so it gets no material below it. Paint
  it; these supports stand on the build plate and snap off easily.
- **Window lintel** (top edge of the screen opening + eyebrow): ~47 mm
  span in the vertical front wall. PETG will bridge it, but the visible
  edge sags; a thin painted strip keeps it crisp.

**Do NOT support** (all designed self-supporting nut-side down — add
blockers if the slicer sneaks anything in):

- D-funnel and boss flanks (≥45° slopes) and the screw counterbores
- Hex nut pocket ceiling (bridges the Ø17 pole-hole ring — proven on the
  base test)
- Teardrop holes (that's what the teardrops are for)
- Band tongue grooves, lid top groove, anti-flex ledge gaps — supports in
  the 0.075 clearances would ruin the confirmed fit; drop blockers in
- Carrier bosses on the band (gusset fins carry them)
- SMA bulkhead, pole and antenna holes (plain round by design — gasket
  seats, and they print vertically here anyway)

## Case back band

No supports at all — it's a constant cross-section arc standing on its
bottom edge; carrier bosses are carried by their 45° gussets and the
insert bosses sit at the bottom. Just give it the 4 mm brim (tall, thin
footprint) and keep it inboard on the sheet so the arc doesn't catch the
nozzle wipe.

## After the print

- 6× M3×4 heat-set inserts: 2 in the band's floor-screw bosses (set from
  the boss undersides) + 4 in the carrier boss pockets.
- Check the band tongue fit dry before inserts — clearances are the
  confirmed 0.075 set; if this batch of PETG shrinks differently, scale
  the band X/Y by +0.1–0.2 % rather than reprinting the shell.
