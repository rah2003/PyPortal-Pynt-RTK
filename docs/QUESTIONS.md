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

8. ❓→📄 **Power shape** — decide in `docs/hardware/power.md` (Phase 0):
   single bank port → Pynt USB with the Lite's 5 V tapped from the Pynt's
   5 V rail (needs verification the rail can source ~160 mA extra), vs.
   two-feed (bank port 2 → Lite 5V_IN directly, common ground). Total
   system ≈ 500–600 mA peak (SAMD51 + TFT backlight + AirLift WiFi bursts
   + F9P ~130 mA + HC977 antenna ~21 mA).
9. 📄 **Pynt D3/D4 label swap** — forum reports say the Pynt's D3/D4
   sockets are silkscreened swapped vs. the classic PyPortal. Verify by
   loopback/scope during Phase 1 bring-up before wiring the Lite.
10. 📄 **D3/D4 SERCOM mapping in Arduino** — `busio.UART(board.D3, board.D4)`
    works in CircuitPython, so the pads are UART-capable; confirm which
    SERCOM/pads the Adafruit SAMD core variant assigns and whether a custom
    `Uart` instance + `pinPeripheral()` is needed. Max reliable baud at
    115200 to be verified in Phase 1.
11. 📄 **TFT driver in 8-bit parallel mode** — PyPortal's ILI9341 is on an
    8-bit parallel bus (SPI only via solder-jumper surgery, not planned).
    Confirm Adafruit_ILI9341's `tft8bitbus` constructor path + pin list for
    the Pynt variant, or fall back to Adafruit_Arcada.
12. 📄 **SPI contention budget** — AirLift (NTRIP + TCP server) and microSD
    (RAWX writes) share the SPI bus. Port the Feather's two-stage-buffering
    stall analysis; measure worst-case SD write latency with WiFi active in
    Phase 1.
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
