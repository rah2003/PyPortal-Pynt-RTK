# Open Questions

Status legend: ❓ needs owner answer · 📄 resolve from vendor docs in Phase 0 ·
✅ answered.

## Answered at kickoff (2026-07-11)

1. ✅ **Toolchain** — PlatformIO Arduino C++ (port Metro/Feather modules).
2. ✅ **v1 scope** — Rover + RAWX logging + Base mode, all in v1.
3. ✅ **Phone link** — SW Maps over WiFi TCP; BLE permanently out (AirLift
   can't do WiFi+BLE together).
4. ✅ **Repo** — public GitHub, `PyPortal-Pynt-RTK`.
5. ✅ **Touch UI depth** — status pages + touch controls; config stays on
   the USB serial menu (no on-screen keyboard in v1).
6. ✅ **Inherited defaults** — Metro's answers carry over wholesale: caster
   `acorn-gnss.net:2101` (mounts `VRS_SouthCentral_RTCM3`/`MS_RTCM3`),
   iPhone hotspot, 12° mask, GPS+GLO+GAL+BDS.

7. ✅ **Same Lite or second Lite?** — Answered 2026-07-11: the Lite +
   antenna **move over** from the RTK-Feather rig. F9P config largely
   carries over (UART1 @115200, constellations); re-verify with u-center
   before first bring-up rather than assuming flash contents.

## Open

8. 📄 **Power shape** — decision framework written
   (`docs/hardware/power.md`): shape A (single bank port, Lite fed 5 V
   from the Pynt's I2C STEMMA port) is preferred **pending bench
   measurements P1–P4**; shape B (two-feed) is the fallback. Closes when
   the measurements are recorded at bring-up.
9. 📄 **Pynt D3/D4 label swap** — forum reports say the Pynt's D3/D4
   sockets are silkscreened swapped vs. the classic PyPortal. Verify by
   loopback/scope during Phase 1 bring-up before wiring the Lite.
10. ✅ **D3/D4 SERCOM mapping in Arduino** — Resolved from
    [variant.cpp](https://github.com/adafruit/ArduinoCore-samd/blob/master/variants/pyportal_m4/variant.cpp)
    (2026-07-11): D3=PA04/D4=PA05, `PIO_SERCOM_ALT` → **SERCOM0 pads
    0/1**, which is free (`Serial1` = SERCOM4 → ESP32, SPI = SERCOM2,
    Wire = SERCOM5). Custom `Uart` recipe in `docs/hardware/wiring.md`;
    Phase 1 loopback is the ground-truth check.
11. ✅ **TFT driver in 8-bit parallel mode** — Resolved on paper
    (2026-07-11): data PA16–23 (pins 34–41) + control pins per variant;
    `Adafruit_ILI9341` `tft8bitbus` constructor (Adafruit's own PyPortal
    Arduino demos use it), `Adafruit_Arcada` as fallback. Verify at first
    compile (Phase 1).
12. 📄 **SPI contention budget** — Analysis written
    (`docs/hardware/platform.md`): 32 KB UART ring + 32 KB SD staging on
    the SAMD51's 256 KB makes the Feather's tight-margin problem roomy;
    closes when Phase 1 measures the real card's worst-case write latency
    with WiFi active.
13. ✅ **Cable stock** — Answered 2026-07-11: cabling will be on hand
    (JST-GH pigtail for the Lite, D3/D4-side leads). Exact connector
    inventory gets confirmed against the Phase 0 wiring diagram.
14. ✅ **microSD card** — Answered 2026-07-11 (photo): PNY Elite 32 GB
    microSDHC, UHS-I **U1** (≥10 MB/s sustained). Plenty of throughput for
    RAWX, but U1 cards can throw long single-write latency spikes — the
    Phase 1 slow-card burst test and the two-stage buffer sizing (Q12) are
    the guard.
15. ✅ **GNSS antenna** — Answered 2026-07-11 (photo): Calian/Tallysman
    **HC977 triple-band + L-band helical**, part 33-HC977-35 (35 dB LNA
    variant), s/n 20200624. Bias 2.2–16 VDC → fine on the F9P's 3.3 V
    antenna feed; ~21 mA typ; IP67; SMA. Helical → **no ground plane
    needed**.
16. ✅ **Enclosure / assembly** — Answered 2026-07-11: enclosure gets
    built **after** the electronics work, and **portrait** is the optimal
    orientation → all touch-UI layout is designed for 240×320 portrait
    (`tft.setRotation()` — which of the two portrait rotations depends on
    cable exit, decided with the wiring diagram).
17. ✅ **RTK-Feather rig post-move** — Mooted by Q7: with the Lite and
    antenna moving over, the Feather rig loses its GNSS and is effectively
    retired as a complete rover (boards remain available as spares).
