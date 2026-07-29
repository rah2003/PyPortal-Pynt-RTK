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
| W2 WiFi provisioning + AP flow | scaffolded (501) — **BLOCKED on W0.5/W0.6** |
| W3 GNSS + Corrections cards | scaffolded (501); settings keys parsed/persisted, gnss_config consumption lands with W3 |
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

## Results (fill during bench)

| Test | Result | Notes |
|---|---|---|
| W0.1 iOS Safari / Android Chrome | | |
| W0.2 accept | | |
| W0.3 parallel curls before failure | | |
| W0.4 /big throughput best/worst | | |
| W0.5 beginAP code + AP IP | | |
| W0.6 AP+BLE / STA+BLE | | |
| W0.7 hostname in DHCP table | | |
