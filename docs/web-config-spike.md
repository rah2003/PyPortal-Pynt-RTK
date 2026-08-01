# Web Config GUI — W0 spike + increment plan

Branch `web-config` (off `coex/nina-fw-airlift` — W5's BLE card needs
the coex stack). Locked decisions:

- **v1 scope = W0–W5** (Status, WiFi provisioning, GNSS, Corrections,
  BLE, Logging, System). W6 (mDNS, sourcetable browser, SD file
  manager, profiles) deferred.
- **AP provisioning entry: touch button only** (TFT WEB page → AP
  button). Never an automatic fallback.
- **Admin auth ON by default.** No hardcoded default passwords: the
  admin password and the AP WPA2 key are **generated on-device (SAMD51
  TRNG) at first web boot** and displayed on the TFT WEB page (PAGE
  button cycles POS → SYS → WEB). User "admin". Changeable from the
  web System card (W5); `adminpass=` / `appass=` in /config.txt.
- **STA web server always-on while surveying**, port **80**. Compile
  flag `ENABLE_WEB_CONFIG` (only `pynt-rover-coex` sets it) exists as
  the gate if the §7.5 concurrency soak ever demands it.
- Web GUI supersedes the previously-planned on-screen-keyboard touch
  work (descoped). The touch UI adds only: the WEB info page, and the
  AP-mode button.

Increment status:

| Increment | State |
|---|---|
| W0 spike | **sketch + this doc ready — needs the bench pass below** |
| W1 status dashboard | implemented (`web_config.cpp`, `tools/webui/`) |
| W2 WiFi provisioning + AP flow | **DONE — full round trip verified on the Pynt 2026-07-31** |
| W3 GNSS + Corrections cards | **DONE — benched on the Pynt 2026-07-31** |
| W4 Logging card | scaffolded (501) |
| W5 BLE + System cards | scaffolded (501) |

## W0 bench (Metro M4 AirLift, custom nina-fw already flashed)

```
pio run -e metro-spike-web -t upload && pio device monitor -b 115200
```

Menu: `1` STA+HTTP · `2` AP+HTTP · `b` BLE toggle · `h` hostname · `s` status.
Needs `firmware/spike/secrets.h` and the hotspot up. Record everything
in the results table at the bottom.

### W0.1 — HTTP serve + gzip-from-PROGMEM (browsers)

`1`, then from a phone browser: `http://<ip>/`.

- [ ] **iOS Safari** renders the dashboard page (tabs visible; footer
      says "connection lost — retrying…" — expected, `/api/status`
      only exists on the rover) → gzip + Content-Length path works
- [ ] **Android Chrome** same
- [ ] `curl -sv http://<ip>/ -o page.html` shows
      `Content-Encoding: gzip` and the file gunzips to the page

### W0.2 — accept behavior for HTTP

The spike uses `server.accept()` (the data-gated `available()` trap is
already proven — coex-bench notes 2026-07-27). Verify HTTP is happy:

- [ ] `nc <ip> 80` then wait 5 s, then type `GET / HTTP/1.1` + Enter
      twice — connection is accepted immediately (server responds after
      the request arrives; no data-gating stall)

### W0.3 — concurrent sockets

From a laptop: `curl -s http://<ip>/api/ping & curl -s http://<ip>/api/ping & curl -s http://<ip>/api/ping & wait`

- [ ] All three return JSON (serialized one-at-a-time is fine — no
      hangs, no connection-refused). Record how many parallel curls
      start failing (nina-fw socket budget; expect ~4-8 usable).
      **This bounds the W1 single-connection design's safety margin.**

### W0.4 — throughput

`curl -so /dev/null -w "%{speed_download}\n" http://<ip>/big` ×5.

- [ ] ≥ 20 KB/s sustained, no stalls/resets (the serial log prints the
      device-side ms for each). Record best/worst. 16 KiB is ~3× the
      whole real UI, so 20 KB/s ⇒ page interactive in ~1 s.

### W0.5 — AP mode on the custom firmware  ← W2 design gate

`2`. Expect `beginAP -> 7` (WL_AP_LISTENING).

- [ ] Phone sees WPA2 network **PyntRTK-setup** (pass `pynt-spike-99`),
      joins, browser loads `http://192.168.4.1/`
- [ ] Record the AP-mode IP the serial prints (expect 192.168.4.1)

### W0.6 — AP + BLE coexistence  ← W2 design gate

Still in AP mode: `b`.

- [ ] Record whether `BLE.begin()` succeeds and the phone (nRF Connect)
      sees **PyntRTK-web-spike** while the AP is up and serving HTTP
- [ ] Also run the STA variant: `1` then `b` (should match Spike B —
      expected PASS)
- Either W0.6 outcome is workable: PASS ⇒ W2 keeps BLE alive in AP
  mode; FAIL ⇒ W2 stops BLE NUS while provisioning (it's a minutes-long
  on-demand mode, entered by button — acceptable). **W2 is not designed
  until this answer is recorded.**

### W0.7 — setHostname

`h`, then `1` to rejoin, then check the router/hotspot's client list.

- [ ] Device appears as `pynt-w0-spike` (or record that the hotspot
      ignores DHCP option 12 — some phone hotspots do; then hostname is
      cosmetic-only and mDNS stays a W6 topic)

## W1 design (implemented this increment)

`firmware/pynt/rover/web_config.{h,cpp}`, eighth superloop module,
polled LAST (lowest-priority residual). Hand-rolled HTTP/1.1 state
machine: `server.accept()` → read request line + headers (bounded 512 B
per pass, 2 s deadline) → route → stream response (≤1 KiB per pass,
`Connection: close`, 8 s deadline) → next connection. One connection at
a time by design; W0.3 tells us how gracefully extras queue.

Routes: `GET /` (gzipped SPA, PROGMEM via
`tools/webui/build_assets.py` → `web_assets.h`, gitignored generated
file), `GET /api/status` (~450 B JSON snapshot of
g_gnss/g_link/g_log + uptime/mode). Everything else: 501 JSON stubs
(`/api/wifi|scan|gnss|ntrip|ble|log|system`) so the surface is visible.
HTTP Basic auth on every route.

New /config.txt keys (existing vocabulary/style, all gated so the
shipping build is byte-identical): `webenable webport hostname
adminpass appass measrate dynmodel nmeagsv` (+ coex-only `bleenable
blename`). Pre-existing keys reused as-is: `wifi1..4`/`pass1..4`
(the plan's wifi2/3 already existed), `ggaperiod` (= the plan's
gga_upload_s), `logubx` (= log_auto), `caster port mount user password
tcpport mode svindur svinacc fixedlat/lon/alt elevmask`.
**Persistence is SD `/config.txt` + optional secrets.h seed — there is
no NVS on this platform** (plan assumption corrected).

## Results (bench run 2026-07-28, Metro/COM8, scripted driver)

| Test | Result | Notes |
|---|---|---|
| W0.1 gzip serve | **PASS** (curl + iOS Safari) | `Content-Encoding: gzip`, 2197 B wire → 5719 B page; owner confirmed the dashboard renders in iOS Safari over the AP. Android: no device on hand — deferred (iOS is the target platform), re-check opportunistically |
| W0.2 accept | **PASS** | 5 s silent connect → request → 200 OK |
| W0.3 parallel curls | **FAIL in spike form** — 0/3, 0/6 | Sequential is solid; simultaneous connects all die against the spike's blocking one-at-a-time handler. Re-characterize against the rover's non-blocking module before drawing design conclusions; the SPA's 1 Hz retry masks transient failures either way |
| W0.4 /big throughput | **PASS** — 57.8–71.2 KB/s | 3× the 20 KB/s bar; whole UI (2.2 KB) ≪ 1 s |
| W0.5 beginAP | **PASS** — code 7 (WL_AP_LISTENING), IP 192.168.4.1 | Owner joined PyntRTK-setup (WPA2) from the iPhone and loaded the page at 192.168.4.1 |
| W0.6 AP+BLE | **PASS (both sides)** — `BLE.begin()` OK + advertising while AP up and serving; owner's nRF Connect saw AND connected to PyntRTK-web-spike while joined to the AP | **W2 unblocked: BLE need not stop during provisioning.** (nRF shows no data stream — correct: the spike advertises a bare peripheral, no NUS service; BLE data was proven on the rover during the §7.5 soak) |
| W0.7 hostname | **BLOCKED** by the `WiFi.end()` finding below | `setHostname()` accepted; rejoin never completed so no DHCP lease to inspect |

### Critical finding — `WiFi.end()` wedges STA rejoin

Two clean experiments, same session:

1. Unconditional `WiFi.end()` before the *first* `begin()` → **every
   join fails** (5/5). Guarding it (`if (netMode) WiFi.end()`) → first
   join succeeds instantly.
2. After a legitimate `end()` (mode switch path) → STA rejoin fails
   **5/5 again**, while `beginAP()` still works (code 7) and BLE is
   unaffected. So on nina-fw 3.0.1 + upstream WiFiNINA 2.1.1,
   **`WiFi.end()` permanently wedges STA-join until module reset**;
   AP bring-up survives.

Consequences:

- The **rover is unaffected** — ntrip.cpp never calls `WiFi.end()`
  (its retry loop just calls `begin()` again).
- **W2's AP↔STA transition must not use `WiFi.end()`.** Design options
  to test: `beginAP()` directly after STA (no end — worked in this
  session's sequence), and STA-after-AP via module reset (drive
  RESETN / SpiDrv re-init) as the exit path from provisioning. A
  reboot-into-STA after saving credentials is also acceptable UX for
  an on-demand provisioning mode and may be the simplest correct
  answer.
- W0.7 retest rides on whichever transition design wins.

## W2 bench — 2026-07-31, on the Pynt (pynt-rover-coex)

Full stack live during all of this: NTRIP connected, 3D fix ~25 SV,
RAWX logging, BLE advertising.

| Check | Result |
|---|---|
| Auth | **PASS** — 401 bare, 200 with creds (per-request recompute verified via serial `adminpass=` override) |
| GET / + /api/status | **PASS** — gzipped SPA + live JSON (all fields incl. ssid/ap) |
| **W0.3 re-test: parallel connects** | **RESOLVED — PASS**: 3/3 and 6/6 parallel requests return 200 against the non-blocking module; the spike's 0/3 was its blocking handler, not the firmware |
| POST /api/wifi | **PASS** — slot 4 saved to SD, listed, cleared |
| GET /api/scan in STA | **FINDING**: `scanNetworks()` during an active association DROPS the STA link (self-heals via the ntrip retry loop in ~30–60 s). Now gated: 409 in STA, allowed in AP mode; SPA surfaces the message. Verified 409 + association intact after the fix |
| W0.7 hostname | `setHostname("pynt-rtk")` applied before join — **owner: check the router's DHCP client table** |
| Boot | Clean with triage markers; hardened TRNG (bounded wait) — first-boot hang did not reproduce |

Flash-recovery note for this Pynt (bootloader quirk, worth knowing):
its UF2 bootloader goes bulk-transfer-dead when left IDLE in bootloader
mode (SAM-BA silent, MSC mounts but reads/writes hang — two different
copy tools, fresh double-taps, known-good cable all failed). The
reliable flash path is the **touch-flow**: boot the app (single reset
if needed), then `pio run -e pynt-rover-coex -t upload --upload-port
<app COM>` — PIO's 1200-touch + immediate bossac write works every
time (~12 s). Avoid parking this board in its bootloader.

Remaining W2 items (owner, phone in hand):
- TFT: PAGE → WEB page shows IP/HOST/ADMN/APSS/APPW; AP button enters
  provisioning (BLE stays up); join `pynt-rtk-setup` with the APPW
  key, browse http://192.168.4.1/, WiFi tab: scan (allowed there),
  save a network, then AP button again = exit + reboot to STA.
- Router DHCP table shows `pynt-rtk` (W0.7 close-out).
- Note: admin password currently overridden to a bench value; clear
  `adminpass=` in /config.txt (or wait for the W5 System card) to
  regenerate a TRNG one.

## W2 AP-flow bench — 2026-07-31, on the Pynt (three sessions, two findings)

**Final design: BOTH AP transitions are reboots.** The TFT AP button
saves a one-shot `bootap=1` to /config.txt and resets; at the next boot
`webConfigInit` clears the flag (crash/power-pull lands in STA), runs
the pre-AP scan (below), then `beginAP()` on the freshly reset module.
Exit = plain reboot. Entry/exit each cost a boot (~2 min with the SD
free-scan) — acceptable for an on-demand provisioning mode.

Two findings forced this design:

1. **The NINA's socket table does not survive `WiFi.end()`.** Switching
   STA→AP live left `WiFiServer::begin()` with no socket (`getSocket`
   starved by the STA session's leaked server/client socks):
   `srvStatus=0` (CLOSED) and `accept()` then returns phantom clients
   (~100/s flood, 38k–49k observed) — nina-fw's `availDataTcp` indexes
   its socket arrays with the host's stale sock number, out of bounds.
   Pre-stopping the server didn't help; only a module reset empties the
   table reliably. (Also seen: `WiFi.status()` leaking STA events —
   `WL_CONNECTION_LOST`, `WL_SCAN_COMPLETED` — while the AP runs, so
   AP-mode serving now ignores the status register entirely.)
2. **`scanNetworks()` kills whatever network is live — both modes.**
   STA: drops the association (recovers via the retry loop). AP: stops
   the AP beaconing permanently (`st` stuck at WL_SCAN_COMPLETED; the
   phone sees the network vanish; owner hit exactly this). The ONLY
   safe window is the AP-entry boot before `beginAP()` — radio idle.
   `/api/scan` now serves a cache filled in that window (12 entries,
   SSID/RSSI/enc); no live scan path exists anymore, in any mode.

**Round-trip verification (owner + wire log):** STA boot → AP button →
reboot → pre-AP scan cached 4 networks → `pynt-rtk-setup` up,
`srvStatus=1` (LISTEN) → phone joined (WPA2 key from TFT), Basic-auth
login, dashboard live over the AP (63 clean accepts ≈ the SPA's 1 Hz
poll) → Scan button returned the cached list instantly → saved the
owner's hotspot to slot 2 (`[web] wifi slot 2 updated`, persisted —
visible in /config.txt and /api/wifi after reboot) → AP button →
reboot → STA rejoined, NTRIP reconnected, GUI reachable at the STA IP.

W2 residuals: W0.7 hostname check (router DHCP table, owner);
Android-browser render (no device on hand); `WEB_AP_DEBUG` triage
prints remain in web_config.cpp behind their flag (off) for future
benches.

## W3 bench — 2026-07-31, on the Pynt

GNSS card: GET/POST `/api/gnss` (measrate 1/2/5 Hz, dynmodel
0/2/3/4/5/6, elevmask 0–45, GSV toggle) — POST persists to SD and
re-applies the full VALSET via the existing gnssRequestModeApply()
path; verified live (2 Hz/pedestrian/15°/GSV-off applied and read
back, then restored to project defaults). gnss_config now consumes
measRateHz/dynModel/nmeaGsv under ENABLE_WEB_CONFIG (shipping build
keeps the fixed 1 Hz/GSV-5 constants, byte-identical).

Corrections card: GET/POST `/api/ntrip` (caster/port/mount/user, blank
password = unchanged and never echoed, GGA period) — POST persists and
calls the new ntripRequestReconnect() (gated): drop socket, retry at
1 s. Verified live: reconnected to the caster with RTCM flowing within
8 s of the POST.
