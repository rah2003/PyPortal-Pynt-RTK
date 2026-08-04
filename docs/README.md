# Docs index

Two kinds of documents live here. **Build docs** are what you read to
assemble, flash, and verify a unit. **History docs** are dated lab
notebooks — the evidence and rationale behind every claim in the build
docs. When they disagree, the newest dated entry wins; please fix the
stale one.

## Read these to build one

| Doc | What it gives you |
|---|---|
| [../README.md → "Build one yourself"](../README.md) | BOM, wiring summary, flash table, first boot, field use — start here |
| [hardware/wiring.md](hardware/wiring.md) | The 4-wire Pynt↔Lite interconnect, the D3/D4 label-swap check, SERCOM0 UART recipe |
| [hardware/power.md](hardware/power.md) | Power tree (shape A adopted: single bank, Lite fed from D3/D4 5 V) |
| [hardware/ucenter-config.md](hardware/ucenter-config.md) | F9P settings verification (what must be true in the receiver) |
| [hardware/checklists.md](hardware/checklists.md) | Per-board bring-up and integration gates, in order |
| [coex-bench.md](coex-bench.md) §3, §7.2, §8 | AirLift firmware backup + flash procedure (one-time prereq for `pynt-rover-coex`), and the rollback path |

## Read these for history and rationale

| Doc | What it records |
|---|---|
| [hardware/bringup-log.md](hardware/bringup-log.md) | Phase 1–3 bench results: label swap confirmed, touch calibration, soak gates |
| [hardware/platform.md](hardware/platform.md) | Bus map, SPI contention analysis, TFT/touch design notes |
| [coex-bench.md](coex-bench.md) | Phase 5 WiFi+BLE coex spikes and soaks; the 2026-08-02 freeze investigation; the 2026-08-03 boot-loop/RAM-starvation postmortem; Phase B regression soak |
| [web-config-spike.md](web-config-spike.md) | Web GUI design gates W0–W5 and their bench results |
| [QUESTIONS.md](QUESTIONS.md) | Every open/answered project question with its closure evidence |
| [reviews/](reviews/) | The 2026-08-03 four-angle team review and the response plan being executed |
