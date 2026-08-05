# Datum & epoch — what frame this rover's positions are in

**Short answer: when RTK-corrected against ACORN, every position this
device produces is NAD83(2011) epoch 2010.00 — not WGS84.** Configure
consuming apps accordingly or eat a silent ~1–2 m systematic offset.

## Operator statement (2026-08-05, closes team-review P1)

The `VRS_SouthCentral_RTCM3` mountpoint broadcasts data referenced to
the standard Alaska ACORN network framework:

- **Datum realization: NAD83 (2011)**
- **Reference epoch: 2010.00**
- Corrections broadcast at 1 Hz to match real-time rover tracking.

Because South-Central Alaska experiences significant tectonic movement,
the network holds the underlying CORS physical base positions fixed to
the National Spatial Reference System (NSRS) 2010.00 epoch. The
network's RTK software shifts the calculated virtual base station
coordinates to this same reference framework, so field data seamlessly
matches historical local control points.

## What that means in practice

- **The rover's NMEA (TCP :10110, BLE NUS) and the TFT/web position are
  in the corrections' frame.** GGA carries no datum tag — any app that
  ingests it decides for itself what frame it's in, and nearly all
  assume WGS84. In SC Alaska, NAD83(2011)→ITRF/WGS84 disagreement is on
  the order of 1–2 m horizontally. RTK precision is cm; mislabeled
  datum silently throws that away.
- **SW Maps / QField**: set the project/layer CRS to
  **EPSG:6318 (NAD83(2011) geographic 2D)** — or the matching Alaska
  State Plane NAD83(2011) zone for projected work — so RTK positions
  overlay correctly on NAD83-based control and parcel data. Do NOT let
  the app treat incoming coordinates as EPSG:4326 when it will be
  combined with local control.
- **OPUS comparability**: OPUS also reports NAD83(2011) epoch 2010.00
  for Alaska. The Phase E field-day comparison (VRS-RTK fix vs OPUS
  static solution on the same mark) is therefore a direct same-frame
  comparison — no HTDP transform needed.
- **Epoch matters here.** SC Alaska moves ~cm/yr; ACORN pinning
  everything to 2010.00 is what makes today's fix line up with
  2010-era NSRS control. Positions compared against ITRF-current-epoch
  products (e.g. PPP) need an HTDP epoch/frame transform first.

## Where the device records it

`datum=` and `epoch=` settings (SD `/config.txt`, serial menu, defaults
`NAD83(2011)` / `2010.00`) — carried as **metadata only**, no transform
is applied. Change them only if the caster/mountpoint changes frames.
Record both in `docs/field-log.md` entries and in RINEX headers when
converting `.ubx` for OPUS.

## Antenna calibration status (P6 lookup, 2026-08-05)

**The HC977 has no NGS calibration and no registered IGS antenna
name.** Verified against the NGS IGS20 composite ANTEX (`ngs20.atx`,
1,556 antennas — no HC-series; Calian/Tallysman's entries are all
patch/VeroStar: TWI3870+GP, TWI3970+GP, TWI3972XF_CONE, TWI7972+GP,
TWIVC6050/6150, TWIVP6000/6050_CONE/6200/6300, TWIVSP6037L) and the
IGS `rcvr_ant.tab` registry (4,550 names — absent). Calian publishes
no phase-center offset in the public datasheet pages either.

Consequences for the Phase E field day:

- **OPUS**: submit with antenna type **NONE**, antenna height **0.0**.
  No PCO/PCV model is applied, so the OPUS position is the antenna's
  **L1 phase center**, not the mark. Tie to the mark separately:
  measured mark→antenna-base height + an estimated base→phase-center
  offset (unpublished; somewhere inside the helix — assume ~±2 cm
  vertical uncertainty unless Calian support provides a number).
- **Score the comparison horizontally.** A helical's horizontal PCO is
  ~mm and averages further over a 4 h occupation — the VRS-RTK vs OPUS
  horizontal comparison is essentially unaffected by the missing
  calibration. Quote vertical with the phase-center caveat.
- The VRS-RTK side has the same unmodeled PCO baked in; for
  centimeter-honest **heights** on future work, the fix is a
  calibrated antenna (e.g. the TWIVP6000-class entries above), not
  better bookkeeping.
