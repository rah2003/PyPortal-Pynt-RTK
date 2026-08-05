# Response plan — Team Review 2026-08-03

Plan of record for working the findings in
[2026-08-03-team-review.md](2026-08-03-team-review.md) (branch
`review/team-review-2026-08-03`, reviewed at `2358dfd`). Ordered so each
phase de-risks the next; finding IDs (H/M/L/P) refer to the review.

## Evidence the review didn't have (2026-08-02/03 bench session)

The plan folds in the freeze investigation that ran the same weekend
(coex-bench.md bench notes 2026-08-02/03):

- **Eight hard freezes, 7–37 min uptime**, on the Jul 31 coex+web build —
  every one with a *second concurrent TCP client* + periodic HTTP probes
  attached; zero freezes (8.36 h, battery-limited) without them. That is
  live field evidence for **M5** (no slow-client drop logic, unbounded
  per-byte drain) and **H3** (socket leak on rejoin) — and the bench
  harness is now a *reproducer* for them.
- **RTK FIXED was reached and held outdoors** (GGA quality 4, 31–32 SV)
  — QB's "never achieved RTK FIX outdoors" is closed; the §7.5 SD gate
  closed with it (86.76 MB, 0 bad checksums, 30,099 epochs at exactly
  1 Hz).
- **Measured runtime datapoint**: ≥8.36 h on the field battery with
  RAWX + NTRIP + TCP + BLE (partially answers QB's field-power gap).
- New findings of our own for Phase A: log-start BUFH spike (24.4 KB of
  the 32 KB file buffer consumed by day-dir create + file alloc), and
  the WDT was already an owner-endorsed action item before the review
  landed.

## Phase A — Pre-field hardening PR (small, bounded, highest leverage)

One PR, every item a few lines, protects everything downstream:

| Item | Finding | Fix |
|---|---|---|
| PVT-age watchdog → fix badge | **H1** (top of list) | stamp `lastPvtMs` in `onPvt()`; clear `f9pDetected` after a few missed nav periods; `fixState()` → NO GNSS |
| SAMD51 WDT + freeze breadcrumb | wedge cluster + our action item 1 | arm at boot, kick per pass; early-warning IRQ stamps which module the pass died in |
| Short `maxWait` on GNSS retry | **H2** | `gnss.begin(SerialGNSS, 250)` in retry path + backoff |
| Server socket lifecycle on rejoin | **H3** | `server->end()` before re-`begin()`; check `server->status()` |
| TCP client drop logic | **M5** (our freeze suspect) | bounded inbound drain budget; honor `write()` returns; drop stalled clients; fix the function-local-static port capture |
| GGA age gate | **M2/P9** | `kGgaMaxAgeMs` check in `maybeSendGga()`; longer first-byte grace than steady-state stale timeout |
| Correction-age honesty | **M6** | grace deadline in its own variable; `lastRtcmMs` stays 0 until real bytes |
| Atomic settings save | **H5** | write `/config.new` → sync → rename |
| Log-start buffer headroom | ours | pre-create day dir at boot; open file eagerly on `log on` |
| One-liners while in there | **L2**, `snprintf` clamps from **L1** | rollover-safe `rebootAtMs`; clamp pattern everywhere |

## Phase B — Regression soak with the freeze harness (validates A)

Re-run the 2026-08-02 observation harness — 2 concurrent TCP clients +
5-min `/api/status` probes + BLE streaming + SD logging — against the
Phase A build. Before A it kills the device in ≤37 min (8/8); after A it
must survive hours, and any residual freeze now self-recovers (WDT) and
self-reports (breadcrumb). Also run the **pinpoint** (listener-only vs
probes-only) if A doesn't obviously close it. This soak doubles as the
§7.5 loop-stability re-measure; add a free-RAM counter to `status`
while instrumenting (closes the last unmeasurable §7.5 box).

## Phase C — Correction/RF instrumentation (Bumble P2, P10, P12)

Enables the field day to answer questions current telemetry can't:

- Enable **UBX-RXM-RTCM** (`msgUsed`, `crcFailed`, `refStationId`) and
  **NAV-RELPOSNED** (baseline length); surface on TFT + `/api/status`.
- Make correction age the *receiver's* age (GGA field 14 already parsed
  and discarded — cheapest source).
- Enable **MON-RF** (AGC/noise/jamming) + **NAV-SAT** C/N0; do the short
  radios-idle vs WiFi+BLE A/B that retires (or confirms) the desense
  hypothesis from the 7-28 FLOAT ceiling — also fixes **P12** (README
  sats/SNR promise) with real data.

## Phase D — Docs pass (parallel with B/C; Honey §6 + QB truth-up)

1. **LICENSE at root** (first — blocks legal reuse).
2. README truth-up sweep: close the items the bench logs already closed
   (Phase 2/3 leftovers, power P1–P4, Q8/Q9/Q12) **plus the new ones —
   RTK FIXED outdoors 2026-08-02, SD-integrity gate, measured ≥8.36 h
   runtime**.
3. "Build one yourself" README section per Honey's outline: BOM w/
   links, wiring summary + label-swap flag, env→purpose→command flash
   table, first-boot walkthrough, field-use quick reference.
4. `docs/README.md` index (build-docs vs history-docs); photos when a
   camera is on site.

## Phase E — Field accuracy day (the headline; Bumble + QB convergent rec)

Prerequisite decisions, forced explicitly before the outing:

- **P1 datum/epoch** — **DONE 2026-08-05**: operator confirmed
  `VRS_SouthCentral_RTCM3` broadcasts **NAD83(2011) epoch 2010.00**
  (ACORN pins CORS to NSRS 2010.00 against SC-AK tectonics);
  `datum=`/`epoch=` settings fields added (metadata, defaults match);
  CRS guidance for SW Maps/QField in `docs/datum-epoch.md` (EPSG:6318).
  Bonus: OPUS reports the same frame/epoch — the field-day comparison
  is direct, no HTDP transform.
- **P6 antenna height** — lookup DONE 2026-08-05: **the HC977 has no
  NGS calibration and no IGS-registered name** (verified in ngs20.atx
  + rcvr_ant.tab; no public PCO from Calian). Protocol set in
  `docs/datum-epoch.md`: OPUS as antenna NONE / height 0 → L1
  phase-center solution, tie to mark mechanically, score the
  comparison horizontally, quote vertical ±~2 cm. Remaining owner
  decision: the physical mount + how mark→base height gets measured
  (`antHeightM` field lands with that).

Then the one-afternoon protocol: occupy a published NGS mark (measured
antenna height), ~4 h RAWX + concurrent VRS-RTK fixes, `.ubx`→RINEX
(`tools/ubx_walk.py` sanity pass → convbin), OPUS submission, compare in
a declared frame/epoch, measure battery draw, write `docs/field-log.md`.
Exercises P1/P6/P8/P11 and validates the VRS itself — the first
truth-check in the project's history.

## Backlog (post-field-day, roughly ranked)

- **H4** RAM reclaim: non-blocking WiFi join (kills the reason for the
  32 KB ring), `SERIAL_BUFFER_SIZE` down, `Serial1`'s 64 KB investigated
  — ~96–120 KB back.
- **Web security cluster**: M3 JSON escaping helper, M4 Host-header/
  custom-header CSRF gate, M7 password length validation, L6 decision
  record (cleartext at rest, TFT display timeout, constant-time compare).
- **Metrology second tier**: P3 survey-in accuracy gate + adopt-RTK-fix
  base workflow; P4 RAWX divisor scaling + MON-COMMS; P5 chunked
  transfer-encoding (or pin Ntrip/1.0 deliberately); P7 `lround` + `_HP`
  fields; P8 hMSL labeling.
- **M1** RAWX drop counter (don't extract when `!fileOpen`).
- Project hygiene: pin WiFiNINA in the shipping envs, `lib_ignore` for
  non-coex envs, log rotation + low-space stop, printHelp() key sweep,
  L4 UI cost gating, L5 `sizeof` capacities.
- **UART2 radio: plan or descope note** — base-mode RTCM relay over
  WiFi (Bumble's second-best) is the likely descope path using existing
  hardware.
- Enclosure (trigger condition fired).

## Sequencing rationale

A is small and protects the field session; B proves A with a reproducer
we already own; C gives the instruments the field day will need; D is
cheap and parallel; E is the payoff and forces the two decisions (datum,
antenna height) that dominate the error budget. Everything in the
backlog is real but doesn't block truth-checking the device.
