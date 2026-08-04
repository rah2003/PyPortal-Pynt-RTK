# PyPortal-Pynt-RTK — Team Review (2026-08-03)

Full-team review of this repo at commit `2358dfd` ("README: retire constraint 3, record Phases 5 + 6 as done"). Four reviewers worked independently, each assigned a blind angle, and the findings were then cross-checked and consolidated. Load-bearing claims were spot-verified against source before consolidation. The firmware review additionally built the firmware at this commit (`pio run -e pynt-rover`, clean; RAM 53.5%, flash 11.4%) and measured symbol sizes from the ELF.

| Reviewer | Angle | Section |
|---|---|---|
| QB | Consolidation + whole-project deep read (firmware, docs, platformio.ini, web tooling) | §1, §2, §5 |
| Fizz | Firmware code review — `firmware/pynt/rover/`, platformio env hygiene, vendored libs | §3 |
| Bumble | GNSS/RTK domain engineering — metrology, receiver config, corrections handling | §4 |
| Honey | Documentation — can a stranger reproduce and use this build? | §6 |

---

## 1. Executive summary

This is an unusually strong hobby-grade build, and all four reviewers said so independently. The bench culture is the standout asset: every claim has a dated log, failures are root-caused (silkscreen swap, touch pin off-by-one, `WiFi.end()` wedge, NINA socket-table leak), and rollback paths are preserved. The bounded-work-per-pass superloop discipline is real, not aspirational. The vendored-lib story (upstream tag + SHA, patch markers, rationale) is better than most commercial firmware trees. The NTRIP client hardening (base64 sign-extension fix, header deadline, stale watchdog) preserves hard-won knowledge.

The findings are concentrated where no bench soak could see them: **metrology, long-horizon reliability, and adversarial input.**

### Where the reviews independently converged

Four reviewers, assigned blind to each other, hit the same walls — that's signal:

- **Stale/absent GGA to the VRS** (QB, Fizz M2, Bumble P9) — never age-gated, never cleared on fix loss, no bootstrap for cold start.
- **"Correction age" lies** (Bumble P2, Fizz M6) — it's link age stamped on byte arrival, pre-seeded on connect; a wrong mount or CRC-failing stream reads as healthy. The receiver's own UBX-RXM-RTCM answer ("was this correction *used*?") is never asked.
- **~131 KB of the 256 KB SRAM is UART ring buffers** (QB, Fizz H4 with ELF symbol proof: `SerialGNSS` + unused `Serial1` = 131,192 B). Half of it buffers a UART the firmware never touches.
- **NINA socket leak on every WiFi rejoin** (QB suspected it; Fizz verified it in WiFiNINA source — `WiFiServer::begin()` never frees the old socket, the table is 10 deep, ~4 hotspot roams kill the phone link and web GUI silently).
- **Web config cluster** (QB + Fizz): CSRF-able state changes, JSON injection via SSID, admin password silently truncated to 16 chars → self-lockout, non-atomic `O_TRUNC` settings save that can wipe all credentials on power loss.
- **A slow TCP client back-pressures the superloop** (QB + Fizz M5) — the header comment promises drop logic that isn't there.

### The unified top-of-list

1. **A dead F9P displays as a live green RTK FIX** (Fizz H1, QB-verified: `f9pDetected` can never go false after init, `lastGgaMs` is write-only, no PVT-age watchdog). Cable works loose mid-survey → operator logs points against a frozen position under a green badge. The worst possible failure shape for this device's purpose.
2. **Datum/epoch undeclared end to end** (Bumble P1). US VRS networks serve NAD83(2011); phone apps assume WGS84/ITRF — up to 1–2 m of silent, systematic error. For a property-lines lineage, this is the entire error budget.
3. **The wedge cluster**: no watchdog anywhere + the socket leak + a 6.6 s blocking stall every ~11.6 s when the F9P is absent (Fizz H2, timed from library source). Field brick, silent link death, frozen UI — all recoverable with a WDT, socket lifecycle handling, and a short `maxWait`.
4. **Metrology gaps** (Bumble P3/P4/P6/P7): survey-in accepts a base 5 m off with no gate; the web GUI's 5 Hz option arithmetically overruns UART1 at 115200 with no MON-COMMS to tell you; no antenna height/ARP anywhere; fixed-base coordinates truncated toward zero instead of rounded.
5. **Docs**: no LICENSE, zero images, no build-one-yourself path, and the README has drifted stale against its own bench logs — several items marked pending were closed 2026-07-19.

---

## 2. Agreed next steps

Three-step sequence, each small and each de-risking the next:

- [ ] **Pre-field hardening PR** (Fizz) — PVT-age watchdog wired to the fix badge (H1), socket lifecycle on rejoin (H3), short GNSS `maxWait` (H2), SAMD51 WDT, GGA age-gate + bootstrap, atomic settings save (H5). All small, well-bounded, protects everything downstream.
- [ ] **Docs pass** (Honey) — LICENSE at root, README truth-up sweep against the bench logs, quick-start restructure (§6). Photos need a camera on site.
- [ ] **Field accuracy day — the headline.** Bumble and QB landed on the same recommendation independently: occupy a published NGS control mark, log ~4 h of RAWX while capturing VRS-RTK fixes concurrently, submit to OPUS, compare in a declared frame/epoch, measure real battery runtime, and write `docs/field-log.md`. One afternoon, no new hardware — and the first time this property-lines device would ever be checked against truth. It also forces the datum question (item 2 above) to be answered explicitly.

---

## 3. Firmware code review (Fizz)


Scope: `firmware/pynt/rover/` + `platformio.ini` + `lib/` vendoring.
Repo state: `main` @ `2358dfd` ("README: retire constraint 3, record Phases 5 + 6 as done").
Requested by QB in RTK Projects thread `f71971c1…`.

Verification: `pio run -e pynt-rover` built clean (exit 0) at this commit.
`RAM: 53.5% (140184 / 262144)`, `Flash: 11.4% (120044 / 1048576)`.
Symbol sizes read from `.pio/build/pynt-rover/firmware.elf` via `arm-none-eabi-nm -S`.
Library behaviour confirmed against the resolved deps in `.pio/libdeps/pynt-rover/`.

---

### High

#### H1 — A dead F9P is displayed as a live RTK FIX

`g_gnss.f9pDetected` is set `false` exactly once, inside `gnssInit()`
(`gnss.cpp:147`), and set `true` at `gnss.cpp:150` / `:182`. Nothing ever
clears it again (`grep f9pDetected` over `firmware/pynt/rover/` — 7 hits,
no other assignment).

There is also no PVT-age watchdog anywhere: `GnssStatus` has `lastGgaMs`
(`shared.h:25`) but it is **write-only** (`gnss.cpp:228` is the only
write; no reader anywhere in the tree), and there is no `lastPvtMs` at all.

So if the Lite is present at boot and then dies, browns out, or the D3/D4
cable works loose mid-session:

- `onPvt()` stops firing, so `g_gnss.carrSoln`, `latDeg`, `lonDeg`,
  `hAccMm` all freeze at their last values.
- `fixState()` (`ui.cpp:66-81`) reads `carrSoln` → header stays **green
  "RTK FIX"** forever.
- `drawPagePos()` (`ui.cpp:144-154`) keeps printing the frozen lat/lon.
- The retry path at `gnss.cpp:177` is gated on `!f9pDetected`, so no
  reconnect is ever attempted — contradicting its own comment
  ("a late-powered or reseated Lite must not require a reboot").

For a survey instrument this is the worst failure shape available: the
operator can log points against a stale position with a green fix badge.

**Fix:** stamp `lastPvtMs` in `onPvt()`; in `gnssPoll()` clear
`f9pDetected` when `millis() - lastPvtMs` exceeds a few nav periods; have
`fixState()` fall through to `NO GNSS` on that condition.

---

#### H2 — F9P-absent retry stalls the superloop ~6.6 s out of every ~11.6 s

`connectGnss()` (`gnss.cpp:112-128`) calls `gnss.begin()` at 115200 and,
on failure, again at 38400. SparkFun v3's `begin()` calls
`isConnected(maxWait)` **three times** (`u-blox_GNSS.cpp:903-925`) and
`kUBLOXGNSSDefaultMaxWait` is **1100 ms**
(`u-blox_external_typedefs.h:59`).

So one failed `connectGnss()` blocks for up to 3 × 1100 × 2 = **6.6 s**,
and `gnssPoll()` (`gnss.cpp:177-187`) re-runs it every 5 s, forever, while
the F9P is missing.

During each stall: the TFT is frozen, `ntripPoll()` is unserviced past
both its 5 s header deadline (`ntrip.cpp:251`) and its 10 s stale
watchdog (`ntrip.cpp:284`), `webConfigPoll()`'s 2 s request timeout
(`web_config.cpp:49`) expires on every in-flight request, and TCP NMEA
clients time out. The no-F9P case is an anticipated state
(`main.cpp:49-51` prints the wiring hint) and is exactly when the operator
most needs a responsive UI.

**Fix:** pass a short explicit `maxWait` to `begin()` in the retry path
(e.g. `gnss.begin(SerialGNSS, 250)`), and/or back the retry cadence off
after the first few failures.

---

#### H3 — Every WiFi reconnect leaks two NINA sockets; the table is 10 deep

`WiFiServer::begin()` in the resolved WiFiNINA
(`.pio/libdeps/pynt-rover/WiFiNINA/src/WiFiServer.cpp:38-43`):

```cpp
void WiFiServer::begin() {
    _sock = ServerDrv::getSocket();      // new socket, unconditionally
    if (_sock != NO_SOCKET_AVAIL)
        ServerDrv::startServer(_port, _sock);
```

It overwrites `_sock` and never releases the previous one.

Both servers re-`begin()` on every rejoin:

- `tcp_nmea.cpp:29-42` — `ensureServer()` sets `serverUp = false` whenever
  `WiFi.status() != WL_CONNECTED`, then calls `server->begin()` again on
  the next pass once the link is back.
- `web_config.cpp:783-793` — identical pattern.

`WIFI_MAX_SOCK_NUM` is **10** (`wl_definitions.h:43`), shared with the
NTRIP client socket and up to 3 TCP NMEA client sockets. At two leaked
slots per drop/rejoin cycle, ~4 cycles exhausts the table. `begin()`
returns `void` and neither caller checks `server->status()`, so the phone
link and the web GUI just stop accepting — silently — until reboot. A
rover on a roaming phone hotspot will hit this.

This is the same failure the project already diagnosed for `WiFi.end()`
and documented at `web_config.cpp:806-812` ("the NINA's socket table
doesn't survive `WiFi.end()` honestly — leaked socks starve `getSocket`,
bind fails, phantom-accept storm"). The identical leak is sitting in the
ordinary reconnect path.

**Fix:** call `server->end()` (or track and `stop()` the old socket)
before re-`begin()`, and check `server->status()` after binding.

---

#### H4 — 131 KB of the 256 KB SRAM is UART ring buffers; half of it is a UART this firmware never uses

`platformio.ini:51` (and `:160` for the coex env) sets
`-DSERIAL_BUFFER_SIZE=32768`. That macro is **global**: the SAMD core's
`RingBuffer` is `RingBufferN<SERIAL_BUFFER_SIZE>`
(`cores/arduino/RingBuffer.h:31-57`) and `Uart` holds **both** an
`rxBuffer` and a `txBuffer` (`cores/arduino/Uart.h:49-50`).

Measured from the built ELF:

| symbol | bytes |
|---|---|
| `SerialGNSS` (`gnss.cpp:18`) | 65,596 |
| `Serial1` (variant.cpp:134 — SERCOM4, the ESP32 UART) | 65,596 |
| **total** | **131,192** |

That is 93.6% of the 140,184 bytes of static RAM in the shipping
`pynt-rover` build, and 50% of the chip's entire SRAM. `Serial1` is never
referenced by this firmware — it is instantiated by the variant and
anchored by its `.init_array` entry, so `--gc-sections` cannot drop it.
Its 64 KB is pure waste. `SerialGNSS`'s 32 KB **TX** ring is near-waste
too: RTCM injection is ~1 kB/s and `Uart::write()` blocks on a full ring
anyway.

`docs/hardware/platform.md:33-36` budgets "a 32 KB UART RX ring buffer and
a 32 KB SD staging buffer" — the actual cost is ~128 KB of rings plus the
32 KB heap file buffer, i.e. ~96 KB more than documented. Runtime total is
~173 KB before stack.

It fits today, but the coex env adds `web_config` (~2.5 KB of statics),
`ble_nus`, and ArduinoBLE/WiFiNINA runtime buffers on top, and the margin
is far thinner than the docs imply.

**Fix (best):** kill the reason the giant ring exists — drive the join
non-blockingly (`WiFiDrv::wifiSetPassphrase()` + poll `WiFi.status()`)
instead of `WiFi.begin()`'s ~10.9 s internal block
(`ntrip.cpp:12-18`, `platformio.ini:47-51`), then `SERIAL_BUFFER_SIZE` can
drop to a few KB and ~120 KB comes back.
**Fix (cheap interim):** drop to 16384 (≈6.4 s of stream at the measured
~2.5 kB/s) and reclaim 64 KB.

---

#### H5 — `settingsSave()` is a non-atomic `O_TRUNC` rewrite, on every setting change

`settings.cpp:82` opens `/config.txt` with `O_WRONLY | O_CREAT | O_TRUNC`
and rewrites it in place. The file holds every WiFi PSK, the NTRIP
credentials, the web admin password, and the AP key.

Power loss anywhere between the `O_TRUNC` and `f.close()` leaves the file
empty or half-written: the device boots to compiled defaults with no WiFi,
no caster credentials, and a regenerated admin password.

This is not a rare window — `settingsSave()` runs on **every** web POST
(`web_config.cpp:287, 368, 418, 449, 481, 528`), every serial `key=value`
(`serial_menu.cpp:117`), and immediately before the AP-mode reboot
(`web_config.cpp:815`). It's a battery-powered field device with a
hardware power switch.

**Fix:** write `/config.new`, `sync()`, `close()`, then `remove()` +
`rename()`. Standard, ~6 lines.

---

### Medium

#### M1 — RAWX is silently discarded whenever the log file isn't open

`sd_logger.cpp:87-107`:

```cpp
if (gnssLogAvailable() == 0) return;
size_t n = gnssLogExtract(chunk, sizeof(chunk));   // <- removed from the buffer
if (n == 0) return;
if (g_log.fileOpen) { ... write ... }              // <- otherwise dropped
```

Data is extracted from the SparkFun file buffer *before* the `fileOpen`
check, so when the file isn't open the chunk is consumed and thrown away
with no counter and no UI signal. That covers:

- the cold-start window from boot until `g_gnss.timeValid`
  (`sd_logger.cpp:31, 137`) — up to ~30 s of RAWX on a cold TTFF, which is
  exactly the data a PPK workflow wants;
- logging toggled off;
- after a write failure (`sd_logger.cpp:99-103`).

`g_log.bufHighWater` is reported but there is no `logDrops` equivalent to
`g_link.bleDrops`.

**Fix:** don't extract when `!fileOpen` unless the buffer is near full,
and add a dropped-bytes counter surfaced on the SYS page and `/api/log`.

#### M2 — A stale GGA is uploaded to the VRS caster indefinitely

`ntrip.cpp:189-195` gates only on `g_gnss.lastGga[0]` being non-empty.
`g_gnss.lastGga` is never cleared on fix loss or F9P loss
(`gnss.cpp:225-229`), and `g_gnss.lastGgaMs` — which exists precisely for
this — is written at `gnss.cpp:228` and **read nowhere**.

Result: after the fix drops or the receiver dies, the rover keeps posting
its last known position upstream every `ggaPeriodS` (default 10 s). A VRS
caster keeps synthesising a base for a location the rover may have left.
Compounds H1.

**Fix:** `if (millis() - g_gnss.lastGgaMs > kGgaMaxAgeMs) return;` in
`maybeSendGga()`.

#### M3 — JSON string injection in every settings-echo route

User-settable strings are emitted into JSON with bare `%s` and no escaping
of `"`, `\`, or control characters:

- `web_config.cpp:258` — `wifiSsid[i]`
- `web_config.cpp:384` — `casterHost`, `casterMount`, `casterUser`
- `web_config.cpp:436` — log filename
- `web_config.cpp:461` — `bleName`
- `web_config.cpp:497` — `hostname`

`scanIntoCache()` at `web_config.cpp:304` explicitly filters `"` and `\`
("crude JSON safety") — the author knew, but only the scan path got the
guard.

Repro: `POST /api/wifi` with `ssid=my"net` → `GET /api/wifi` returns
`{"slots":[{"slot":1,"ssid":"my"net",…` → `JSON.parse` throws and the SPA's
WiFi card dies. Depending on how the SPA renders these, `</script>` in a
hostname is a stored-XSS candidate too. Authenticated-only, so Medium.

**Fix:** one `jsonStr()` helper that escapes on the way out; use it
everywhere, including the scan path.

#### M4 — No CSRF protection on the config API

`route()` (`web_config.cpp:536-595`) checks Basic auth and nothing else —
no `Origin`/`Host` validation, no token, no custom-header requirement. The
POST bodies are `application/x-www-form-urlencoded`
(`formField()`, `web_config.cpp:159-178`), which is a CORS *simple*
request: no preflight, so the request is sent and the side effect lands
even though the response is blocked.

Browsers attach cached Basic credentials per-origin regardless of who
initiated the request, so once the operator has authenticated to the
device in that browser, any page they subsequently load can silently
`POST /api/system` (change the admin password), `POST /api/wifi` (wipe a
slot), `POST /api/ntrip` (repoint the caster), or `POST /api/reboot`.

Chrome's Private Network Access checks blunt this on the public→private
path, but not private→private — and AP provisioning mode is exactly
private→private with the phone joined to the device's own AP.

**Fix:** reject requests whose `Host` header isn't the device's own
IP/hostname, or require a non-simple header (e.g. `X-Pynt: 1`) which
forces a preflight the attacker's page can't satisfy.

#### M5 — `tcp_nmea` doesn't do what its header comment says

Header (`tcp_nmea.cpp:5-9`) claims inbound bytes "are drained and
discarded so a chatty client can't wedge the socket buffers" and "a slow
or stalled client gets dropped rather than back-pressuring the loop."
Neither is bounded in the code:

- `tcp_nmea.cpp:109` — `while (clients[i].available()) clients[i].read();`
  is unbounded, one SPI round-trip per byte. A chatty client *is* the
  stall.
- `tcp_nmea.cpp:123` — `clients[i].write(...)` return value is ignored;
  there is no timeout, no failure count, and no drop logic. A wedged
  client back-pressures the loop directly.

Separately, `tcp_nmea.cpp:35` — `static WiFiServer srv(g_settings.tcpPort)`
is a function-local static, so the port is captured on the *first* call
and a later `tcpport=` change never takes effect, even across the
`serverUp` cycle (which only re-`begin()`s the same object — see H3).

#### M6 — Correction age reads a healthy "0 s" before any RTCM has arrived

`ntrip.cpp:260` pre-seeds `g_link.lastRtcmMs = millis()` on caster connect
as a grace period. `correctionAgeMs()` (`shared.cpp:8-11`) only reports
`UINT32_MAX` when `lastRtcmMs == 0`. So for the first 10 s of every caster
connection the TFT `CORR` line (`ui.cpp:169-173`) and `/api/status`'s
`corr` field both show **0 s** — the healthiest possible reading — while
zero corrections have actually been received.

**Fix:** keep the grace deadline in its own variable; leave `lastRtcmMs`
at 0 until real bytes land.

#### M7 — Admin password silently truncated to 16 chars, with no minimum

`web_config.cpp:511-513` reads up to 32 chars into `v[33]` then
`strncpy(g_settings.adminPass, v, 16)`. A user who sets a 20-character
password is locked out on the next request, with a `{"ok":1}` response.

The AP key enforces `strlen(v) >= 8` (`web_config.cpp:517`); the *admin*
password enforces nothing, so a 1-character admin password is accepted —
and there is no rate limiting or lockout on the Basic auth path.

**Fix:** validate length against `sizeof(adminPass)-1`, enforce a minimum,
and return the stored length so the UI can warn.

---

### Low / latent

#### L1 — Unclamped `snprintf` return used as a length, and the `sizeof(buf) - n` underflow pattern

`snprintf` returns what *would* have been written. Two consequences, both
currently unreachable at today's field sizes but both one-line fixes:

- `respondJson(status, n)` (`web_config.cpp:196-199`) passes `n` straight
  through to `rspBodyLen`. If any status string grows past `jsonBuf`'s 900
  bytes, `pollRespond()` (`web_config.cpp:694`) reads past the end of
  `jsonBuf` and writes adjacent BSS onto the wire.
- The accumulator form — `web_config.cpp:255-267` (`respondWifiListJson`,
  no guard) and `ntrip.cpp:139-150` — does
  `n += snprintf(buf + n, sizeof(buf) - n, …)`. Once `n > sizeof(buf)`,
  `buf + n` is out of bounds and `sizeof(buf) - n` underflows to a huge
  `size_t`, i.e. an unbounded write.

`respondScanJson()` (`web_config.cpp:333`) *does* guard. Worth applying
the same clamp everywhere: `if (n < 0 || n >= (int)sizeof(buf)) n = sizeof(buf) - 1;`

Current headroom checked: NTRIP request ≈ 396 / 512 bytes; wifi-list JSON
≈ 390 / 900; status JSON ≈ 300 / 900. Safe now, fragile to any field
widening or a fifth WiFi slot.

#### L2 — `rebootAtMs` is the one rollover-unsafe `millis()` comparison in the tree

`web_config.cpp:752` uses `millis() > rebootAtMs`. Everywhere else the
codebase correctly uses the `millis() - t >= d` idiom. At ~49.7 days
uptime a reboot request scheduled across the wrap is either ignored
forever or fires instantly.

#### L3 — The RTCM drain loop is the only unbounded one in the superloop

`ntrip.cpp:277-283` drains `while (sock.available())` with no per-pass
byte budget, while every other module documents "one bounded chunk per
pass". The NINA's own socket buffer caps it in practice, so this is a
consistency/robustness nit rather than a live stall — but after a
10.9 s `WiFi.begin()` block the backlog is real.

#### L4 — UI cost dominates the loop

- `uiPoll()` calls `handleTouch()` → `ts.getPoint()` on **every** pass
  (`ui.cpp:313`), which reconfigures four pins and does multiple
  `analogRead`s. It should be gated to ~50 Hz like the redraw is.
- The 2 Hz redraw (`ui.cpp:333-338`) repaints every line unconditionally —
  ~10 × `fillRect(0, y, 240, 20)` (`ui.cpp:116`) plus text, with no
  dirty-value check. This is almost certainly what the `[main] worst loop`
  report is measuring.

#### L5 — Field capacities are magic numbers duplicated from `settings.h`

`settings.cpp:157-194` hardcodes `33`, `65`, `49`, `17`, `25` in
`setStr()` calls instead of `sizeof(g_settings.field)`. They're all
correct today; changing any field width in `settings.h` silently
overflows. Same for the `strncpy(..., 64)` / `[64] = '\0'` pairs
throughout `web_config.cpp:283-286, 394-413, 478-480, 506-519`.

#### L6 — Credentials at rest and on screen

Not bugs, but worth an explicit decision record:

- `/config.txt` stores WiFi PSKs, the NTRIP password, the web admin
  password and the AP key in cleartext on a removable card
  (`settings.cpp:90, 96, 119, 122`).
- The TFT WEB page displays the admin password and AP WPA2 key
  permanently, with no timeout (`ui.cpp:231-233`). Documented as a locked
  decision; a reveal-on-tap with a 30 s timeout would keep the "no
  hardcoded defaults" property without leaving admin creds visible to
  anyone with line of sight.
- HTTP Basic over plaintext `:80` (`web_config.cpp:130-133`), compared
  with `strcmp` (non-constant-time; impractical to exploit over WiFi, but
  a `memcmp`-style constant-time compare is free).

#### L7 — Nits

- `webConfigApRequested()` and `webConfigApActive()` are identical
  (`web_config.cpp:829-830`); `ui.cpp:88` uses the "requested" name, which
  implies a pending state that doesn't exist.
- `sd_logger.cpp:67` says "27 filename collisions"; the loop is `a`–`z`,
  so 26.
- `respond501()` (`web_config.cpp:214-218`) is now dead — all routes are
  implemented.
- `printHelp()` (`serial_menu.cpp:18-30`) omits keys that
  `settingsApplyKeyValue` accepts: `blename`, `bleenable`, `adminpass`,
  `appass`, `webport`, `hostname`, `measrate`, `dynmodel`, `nmeagsv`.

---

### platformio.ini + vendoring

**Good.** The vendored-lib story is genuinely exemplary — both
`lib/ArduinoBLE/VENDORED.md` and `lib/Arduino_SpiNINA/VENDORED.md` record
upstream tag *and* commit SHA, exactly what was patched and why, the
in-source `AIRLIFT PATCH` marker, the enabling define, and the facts the
project depends on. This is better than most commercial firmware trees.
`features.h:17-33`'s `#error` gates are the right way to keep the coex
defines honest.

**Verified:** `ESP32_GPIO0 = 6` / `ESP32_RESETN = 7` / `SPIWIFI_SS = 8` /
`SPIWIFI_ACK = 5` in `variants/pyportal_m4/variant.h:130-136` match
`firmware/pynt/rover/pins.h:32-43`, so `ntrip.cpp:200-205`'s claim that the
`COEX_UPSTREAM_NINA` path picks up the same pins from variant macros
holds. `pins.h`'s `#ifndef` guards mean the variant wins where both
define, which is the safe direction.

**Nit:** `lib/` is global to all environments, so the vendored ArduinoBLE
is compiled for `pynt-rover`, `pynt-bringup`, and every spike env. I
checked the linked output — **zero** `BLELocalDevice`/`HCITransport`
symbols survive `--gc-sections` in `pynt-rover`, so the README's
"byte-identical rollback path" claim holds functionally; the only cost is
build time. Adding `lib_ignore = ArduinoBLE, Arduino_SpiNINA` to the
non-coex envs would make the intent explicit.

**Dismissed:** repeated `gnss.begin()` in the retry path does *not* leak
the 32 KB file buffer — `createFileBuffer()` bails when
`ubxFileBuffer != nullptr` (`u-blox_GNSS.cpp:7577`).

---

### What's genuinely good

- The bounded-chunk discipline (`sd_logger.cpp:87`, `ble_nus.cpp:111`,
  `web_config.cpp:637, 671, 691`) is real and consistently applied — the
  RTCM drain (L3) is the only exception.
- The HTTP parser is careful: `reqLine`, `hdrLine`, and `body` are all
  bounds-checked on write (`web_config.cpp:644, 660, 672`), and the
  negative-`Content-Length` case is caught by the `contentLen >= sizeof(body)`
  guard at `:624` because of the `size_t` wrap. I found no unguarded write
  in it.
- `base64enc()`'s unsigned widening (`ntrip.cpp:81-83`) with the comment
  explaining that signed-char sign-extension causes a permanent 401 — that
  is hard-won knowledge, correctly preserved.
- Comments consistently record *why* and cite the bench date that proved
  it. The `web_config.cpp:76-86` scan-cache rationale and the
  `:804-819` AP-transition rationale are the kind of notes that stop a
  future maintainer from re-breaking it.


---

## 4. GNSS/RTK domain review (Bumble)


Reviewed at `REPOS/PyPortal-Pynt-RTK` @ `2358dfd` (main). Angle assigned by QB:
GNSS engineering, not C++ style. Files read: `README.md`,
`firmware/pynt/rover/{gnss_config,gnss,ntrip,shared,settings}.{cpp,h}`,
`docs/hardware/ucenter-config.md`, `docs/coex-bench.md`, `docs/QUESTIONS.md`.

### Verdict

The GNSS layer is competently built and the config choices are mostly
defensible — several match SparkFun/u-blox practice exactly. The gaps are
concentrated in the **metrology** layer, not the plumbing: the build measures
whether bytes are flowing, but never measures whether the corrections are
being *used*, what *datum* the answers are in, or how the position compares
to truth. For a build whose lineage is "SWMaps-propertylines," that's the
consequential omission.

### Verified correct (checked, not assumed)

- **RTCM 1230 present** at rate 5 in the base set. Required, not optional:
  u-blox states the correction stream must contain 1230 or 1033 "otherwise
  the GLONASS ambiguities can only be estimated as float, even in RTK fixed
  mode." Many homebrew bases miss this.
- **Constellation set** GPS+GLO+GAL+BDS is the ZED-F9P maximum; the RTCM
  MSM4 set (1074/1084/1094/1124) matches it one-for-one. SBAS/QZSS off is
  right for CONUS RTK.
- **12° elevation mask** is defensible. F9P default is 10°; 15° is a common
  community tightening for RTK robustness. Not a defect.
- **MSM4 over MSM7** is the correct call for a bandwidth-limited radio link.
- **RAWX + SFRBX** is the correct pair for PPK (SFRBX carries the nav data).
- `CFG-NMEA-HIGHPREC=1` is on — required, or SW Maps/QField silently round
  away the RTK precision.
- `applyRoverTmode()` explicitly clears `TMODE_MODE`, `UART2OUTPROT_RTCM3X`
  and NAV-SVIN on the rover switch. Latched-TMODE-after-base is a classic
  field failure; this build already guards it.
- `/config.txt` persists `fixedlat/lon` at **9 decimals** (`settings.cpp:105`)
  — precision is preserved on disk. The loss is downstream, at VALSET (P7).

### Findings, prioritized

#### P1 — Datum and epoch are undeclared end to end
Nothing in firmware, settings, docs, or the `.ubx` log records which
reference frame the output coordinates are in. US commercial VRS networks
broadcast **NAD83(2011) epoch 2010.00**; SW Maps and QField will treat the
incoming NMEA as WGS84/ITRF. **NAD83(2011) and WGS84(G1762) differ by up to
one or two meters in CONUS.** For property lines that is the entire error
budget, silently. RTCM 1005/1006 carries no datum identifier, so the receiver
cannot tell you — it has to be declared by the operator.

*Fix:* add a `datum=` / `epoch=` settings field, require it when a caster is
configured, stamp it into the log filename or a sidecar `.txt`, and document
the matching CRS to set in SW Maps/QField. Confirm the broadcast frame from
the acorn-gnss sourcetable or the network operator — do not assume.

#### P2 — No receiver-side correction diagnostics; "correction age" is link age
`correctionAgeMs()` (`shared.cpp:8`) returns `millis() - lastRtcmMs`, where
`lastRtcmMs` is stamped when **bytes arrive from the caster**
(`ntrip.cpp:281`). That is TCP liveness, not solution health. A caster
streaming the wrong mount, a CRC-failing stream, or corrections for a station
1000 km away all read as "age 0 ms."

The canonical instruments are absent from the whole tree (grep: zero hits):
- **UBX-RXM-RTCM** — reports per-message `msgType`, `crcFailed`,
  `msgUsed` (2 = used), and `refStationId`. This is the direct answer to
  "are the corrections actually being applied," and it catches wrong-mount
  and CRC problems the byte counter cannot.
- **UBX-NAV-RELPOSNED** — baseline N/E/D and length, i.e. how far you
  actually are from the (possibly virtual) base, plus `diffSoln`/`carrSoln`
  flags. Sanity-checks the VRS cell assignment.
- **GGA field 14** (age of differential data) — already being parsed for the
  NTRIP upstream in `gnss.cpp:55`, and thrown away. Cheapest possible fix.

*Fix:* enable RXM-RTCM + NAV-RELPOSNED on UART1, surface
`refStationId` / `crcFailed` count / baseline length on the TFT and the web
status card, and make the correction-age readout the *receiver's* age.

#### P3 — Survey-in default permits a base up to 5 m off
`svinAccLimit01mm = 50000` = **5 m** (`settings.h:37`). The Phase 3 spot
check converged at 1.46 m and was recorded as a pass. That 1.46 m is an
absolute bias transferred to every rover fix taken from that base. SparkFun
RTK Everywhere ships the same 60 s / 5 m u-blox-derived defaults, but adds a
gate this build lacks: "the device will wait for the position accuracy to be
better than 1 meter before a Survey-In is started."

Also missing: the mature workflow for an accurate base. Emlid/SparkFun-class
products let a base first operate as an NTRIP rover, reach RTK FIX, and adopt
that as its fixed position — or PPP the base's own RAWX log (OPUS) and enter
the result as fixed LLH. This device already has both halves (NTRIP client +
RAWX logger) and never connects them.

*Fix:* tighten the default accuracy limit, add the pre-survey accuracy gate,
and add a "adopt current RTK position as fixed base" path. Stop reporting a
survey-in as a pass without stating the achieved accuracy as an error budget.

#### P4 — The web GUI's 5 Hz option overruns UART1 at 115200
`gnss_config.cpp:63` accepts `measRateHz` 1–5, while
`UBX-RXM-RAWX` stays at MSGOUT rate 1 — every epoch (`gnss_config.cpp:35`).
RAWX payload is `16 + 32 × numMeas` bytes; a ZED-F9P with four
constellations dual-band has been observed at 65 measurements (~2.1 kB per
epoch with framing). At 5 Hz that is ~10.5 kB/s of RAWX alone, plus NMEA
(~2.4 kB/s at 5 Hz) plus NAV-PVT plus SFRBX ≈ **~13.8 kB/s against the
11.52 kB/s that 115200 8N1 can carry.** 3 Hz is already ~73% and bursty.
The measured 1.12 kB/s in the coex soak was a poor-sky-view bench antenna —
open sky roughly doubles `numMeas`, so the headroom is smaller than the log
suggests. Secondary effect: SD write load scales the same way, and
`platform.md`'s contention math was done for ~10 MB/h.

Note also that nothing would *tell* you: **UBX-MON-COMMS / MON-TXBUF** (TX
buffer usage, overrun/error counters) are absent, so an over-subscribed port
manifests as silently missing epochs in the `.ubx`.

*Fix:* scale the RAWX/SFRBX MSGOUT divisor with `measRateHz` so raw logging
stays at 1 Hz while nav runs faster (one-line change — set the divisor to
`measRateHz`). Add MON-COMMS at 1/10 Hz and surface TX errors. Raising baud
is the alternative but `wiring.md`'s 1 kΩ series resistors argue against it
without scoping.

#### P5 — NTRIP 2.0 chunked transfer-encoding is unhandled
The client advertises `Ntrip-Version: Ntrip/2.0` and accepts
`HTTP/1.1 200` (`ntrip.cpp:142,170`), but **every NTRIP 2.0 component must
be able to handle chunked transfer encoding**, and the header parser never
looks for `Transfer-Encoding`. If a v2 caster chunks the stream, hex chunk
lengths and CRLFs are injected straight into `gnss.pushRawData()`. RTCM3
framing resyncs on `0xD3` so it degrades rather than dies — which is worse,
because it looks like a marginal link. Today's caster evidently doesn't chunk;
the next one may.

*Fix:* detect `Transfer-Encoding: chunked` in `headerPoll()` and de-chunk in
the `Connected` state, or drop the advertised version to Ntrip/1.0 and stay
on the rev1 path deliberately.

#### P6 — No antenna reference point or antenna height anywhere
There is no ARP offset, no slant/vertical antenna height, and no phase-centre
handling in settings, base config, or the log. Consequences:
- Base mode broadcasts **1005** (ARP, no antenna height) rather than **1006**
  (ARP *with* antenna height). Legal, but it pushes the height reduction
  entirely out-of-band with nothing recording it.
- 1007/1008/1033 antenna descriptors absent, so a rover cannot apply an
  antenna model.
- The HC977 is a helical with a non-trivial phase-centre height above its
  mount. I could not find an NGS calibration for the HC977 in search — that
  needs a direct query of the NGS antenna calibration database. If it is
  uncalibrated, absolute vertical carries a cm-level unknown and OPUS
  submissions must use a generic antenna.

*Fix:* add `antHeightM` to settings, apply it (1006, or reduce the fixed
position to the mark), record it in the log sidecar, and resolve the HC977
calibration question before trusting any vertical number.

#### P7 — Fixed base LLH is quantized *and truncated*
`gnss_config.cpp:108` writes `CFG-TMODE-LAT/LON` at 1e-7 deg (~1.1 cm) and
deliberately skips the `_HP` 1e-9 refinements; height goes in as cm with no
`HEIGHT_HP`. The code comment owns the tradeoff — fine — but the C cast
`(int32_t)(fixedLonDeg * 1e7)` **truncates toward zero**, so at a Texas
longitude of ~-97.7 the error is biased consistently eastward by up to
1.1 cm rather than being centred. `lround()` halves it for free; populating
the `_HP` fields eliminates it. Since the settings file already holds 9
decimals, the precision exists and is being discarded at the last step.

#### P8 — hMSL is displayed as elevation
`g_gnss.hMslM` (`gnss.cpp:91`) is shown on the POS page. u-blox derives MSL
from an internal EGM96 grid at **10°×10° spacing, rounded to 1 m**, bilinearly
interpolated — it is a nav-grade convenience, not an orthometric height. Next
to a 1.4 cm hAcc readout it reads as survey-grade and isn't.

*Fix:* show ellipsoidal height as the primary number (or label hMSL as
approximate), log ellipsoidal height, and note that orthometric conversion
needs GEOID18 in post.

#### P9 — No GGA bootstrap for the VRS; cold start can fight the watchdog
`maybeSendGga()` (`ntrip.cpp:189`) returns early unless `lastGga[0]` is set,
and nothing persists a last-known position across power cycles. On a cold
start with no fix, the F9P emits a null-position GGA that many VRS casters
will not accept for cell assignment — so no corrections flow, and the 10 s
`kCorrectionStaleMs` watchdog (`ntrip.cpp:284`) tears the socket down and
backs off. Mature products (Emlid, SparkFun) let you configure an approximate
position and synthesize the bootstrap GGA. The 10 s window is also tight for a
VRS recomputing a cell after a boundary crossing.

*Fix:* persist last-known lat/lon, allow a manual approximate position, and
give the caster a longer grace period before the first RTCM byte than the
steady-state stale timeout.

#### P10 — Coex was validated on links, never on RF
`docs/coex-bench.md` §7.5 is a good soak, but it records zero C/N0, AGC, or
noise figures. The one RTK-relevant result — 3,506 s FLOAT, fix never reached
— is attributed to "bench antenna sky view" with no supporting measurement,
while WiFi and BLE radios were transmitting inches from a GNSS antenna. That
explanation may well be right; it is currently a hypothesis, not a finding.
**UBX-MON-RF** (per-band AGC, noise level, jamming indicator) and NAV-SAT
C/N0 are absent from the tree.

*Fix:* enable MON-RF, log AGC/noise/jamming, and re-run a short A/B —
radios idle vs. WiFi+BLE streaming — comparing mean C/N0 and AGC. That either
retires the concern with data or finds a real desense problem.

#### P11 — The PPK story doesn't close
RAWX logging is correctly implemented, but the configured caster mount is a
**VRS** (`VRS_SouthCentral_RTCM3`). A virtual reference station has no
physical observations, so there is no base RINEX to post-process against. The
logs are useful for PPP/OPUS (single-point) and for PPK against a real CORS
station pulled separately from NGS — not against this mount. Repo has no
RINEX conversion note (`convbin`/RTKCONV) and no marker/antenna metadata to
fill a RINEX header.

#### P12 — README promises sats/SNR the firmware doesn't have
README's touch-UI description lists "sats/SNR." Grep finds no NAV-SAT, no
C/N0 field, and no local GSV parsing — only `numSV` from NAV-PVT. GSV is
forwarded to the phone but never parsed on-device. Either enable NAV-SAT (it
is the right source, and pairs with P10's RF work) or correct the README.

#### L-band fallback — architecturally blocked, worth stating explicitly
The HC977 receives L-band, but the ZED-F9P has no L-band demodulator; that
requires a NEO-D9S companion, which needs a port — and UART2 is
constitutionally reserved. The realistic path is **SPARTN over IP** (the F9P
accepts SPARTN on UART1 with `CFG-UART1INPROT-SPARTN`, currently off) using
the WiFi the device already has. That trades the VRS dependency for an MQTT+TLS
client on a NINA-W102, which is a real cost. Worth documenting as a considered
and deferred option rather than an open question.

### Recommended next step

**Occupy a published NGS control mark and validate absolute accuracy against
OPUS — then instrument what the validation exposes.**

One afternoon, no new hardware:
1. Set the rover on a published NGS mark, measured antenna height recorded.
2. Log 4 h of RAWX; capture RTK fixes from the VRS concurrently.
3. Submit the `.ubx`→RINEX to OPUS; compare the OPUS solution against the
   VRS-RTK coordinates in a stated frame and epoch.

That single test exercises P1 (datum/epoch), P6 (antenna offset), P8 (geoid),
P11 (the PPK/RINEX toolchain), and the VRS itself — and it is the one thing a
property-lines device has never been checked against. Everything upstream is
plumbing validation; this is the first accuracy validation.

Pair it with the P2 instrumentation (RXM-RTCM + NAV-RELPOSNED + GGA field 14),
because the test will raise questions the current telemetry cannot answer.

Second-best next step, if a feature is wanted over a measurement: **base-mode
RTCM relay over WiFi.** Base mode currently emits RTCM3 only on UART2 for a
radio that doesn't exist yet. Enabling RTCM3 on UART1 as well would let the
MCU push corrections to a caster (NTRIP server mode) or serve them over
TCP/BLE — a real base station, using hardware already in the box, without
touching the UART2 constraint.

### Sources

- [ZED-F9P Integration Manual (UBX-18010802)](https://content.u-blox.com/sites/default/files/ZED-F9P_IntegrationManual_UBX-18010802.pdf) — RTCM 1230/1033 requirement for GLONASS fixing; 1005 vs 1006; supported RTCM input list
- [ZED-F9P Interface Description](https://cdn.sparkfun.com/assets/learn_tutorials/8/5/6/ZED-F9P_UBX_NMEA_and_RTCM_protocols.pdf) — UBX-RXM-RTCM, NAV-RELPOSNED, MON-RF/MON-COMMS, CFG-TMODE keys, UPD-SOS (protocol 27)
- [UBX-RXM-RTCM field semantics (ublox_dgnss)](https://docs.ros.org/en/kilted/p/ublox_dgnss_node/generated/program_listing_file_include_ublox_dgnss_node_ubx_rxm_ubx_rxm_rtcm.hpp.html) — `crcFailed`, `msgUsed=2`, `refStationId`
- [SparkFun RTK Everywhere — Base Menu](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_base/) — 60 s / 5 m survey-in defaults; "wait for accuracy better than 1 meter before Survey-In is started"; ECEF or geographic fixed entry
- [SparkFun RTK Everywhere — GNSS Menu](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_gnss/) — elevation mask semantics
- [BKG / IGS NTRIP](https://igs.bkg.bund.de/ntrip/) — NTRIP 2.0 components must handle chunked transfer encoding
- [ITRF2014, WGS84 and NAD83 — PSU GEOG 862](https://courses.ems.psu.edu/geog862/node/1804) — NAD83(2011) vs WGS84(G1762) differ up to 1–2 m in CONUS
- [WGS84 vs NAD83 vs ITRF2014 — Point One](https://pointonenav.com/news/wgs84-vs-nad83-vs-itrf-014/) — US VRS networks broadcast NAD83(2011) epoch 2010.00
- [u-blox portal: geoid model used](https://portal.u-blox.com/s/question/0D52p00008HKDSkCAP/what-geoid-model-is-used-and-where-is-this-calculated) — EGM96 10°×10° grid rounded to 1 m, bilinear interpolation
- [u-blox portal: RXM-RAWX max length](https://portal.u-blox.com/s/question/0D52p00008cjYUiCAM/) — 65 measurements / 2014 bytes observed on ZED-F9P
- [SparkFun u-blox library `u-blox_structs.h`](https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library/blob/main/src/u-blox_structs.h) — RAWX = 16 + 32 × numMeas
- [ArduSimple: how to configure ZED-F9P](https://www.ardusimple.com/how-to-configure-ublox-zed-f9p/)
- [Calian HC977 datasheet](https://www.tallysman.com/app/uploads/2020/03/Tallysman%C2%AE-HC977-Datasheet.pdf) — bands, no ground plane; no NGS calibration reference found


---


## 5. Project-level review (QB)

Deep read of the rover firmware, all of `docs/`, `platformio.ini`, and the webui tooling. Firmware-level findings were merged into §3 during consolidation; what follows is the project-level material.

### Omissions at project level

- **The "field-proven" claim is bench-only.** The coex+web build has never achieved RTK FIX outdoors (the 75-min soak topped out at FLOAT from bench sky view), and no position has ever been validated against a known point, OPUS/PPP, or a repeatability test.
- **README has gone stale against its own bench logs**: Phase 2 "touch buttons + BUFH unverified" and Phase 3 "`b_` rotation re-check pending" were both closed 2026-07-19 in `bringup-log.md`; power P1–P4 are done but README/`power.md` still say pending; `QUESTIONS.md` Q8/Q9/Q12 met their closure conditions but were never closed.
- **No LICENSE, no photos** (Honey covered these — confirmed independently).
- **UART2 radio: zero plan.** It is the project's central architectural constraint, base mode already configures RTCM3 onto it, yet no radio selection, link budget, or base→rover end-to-end test exists. Corrections from base mode have never been consumed by anything.
- **Field power story is thin** — the load table is vendor estimates, with no measured system draw or runtime on the actual bank; no brownout detection (SAMD51 BOD33), and a sagging pack mid-SD-write is the classic FAT-corruption path.
- **Error surfacing**: NTRIP 401 vs. no-coverage vs. VALSET NAK are indistinguishable on the TFT — serial-only. A field operator can't self-diagnose.
- **Unvalidated config paths** — `dynmodel`/`elevmask` from `/config.txt` or the serial menu can NAK the entire batched VALSET transaction, silently reverting all nav settings (the web path validates; file/serial don't).
- **WiFiNINA git dep is unpinned** in `platformio.ini` despite the comment claiming otherwise — builds aren't reproducible. The coex env's `#2.1.1` shows the right pattern.
- No log rotation (midnight crossing, low-space stop threshold), no mDNS (W6, known).

### Ways to improve (ranked)

1. **Pre-field hardening mini-PR**: WDT + slow-TCP-client drop + GGA staleness gate (now scoped as §2 item 1). Small, surgical, directly protects the field session.
2. **Docs truth-up sweep + LICENSE + wiring photos**: cheap, and right now the docs contradict each other on what's open.
3. **One residuals bench sit** (~30–60 min): forced WiFi-rejoin RAWX-gap check, convbin the soak file, regenerate the bench admin password, fill the coex-bench §7.5 latency blank.
4. **Reclaim the ~96 KiB of dead RAM** and pin WiFiNINA.
5. Then: enclosure (its trigger condition — "after the electronics" — has fired), and a UART2 radio plan or an explicit descope note.

---


## 6. Documentation review (Honey)

Writer's angle: **can a stranger reproduce and use this build?**

**Bottom line:** the engineering docs are excellent as a *lab notebook* — precise, dated, sourced (`wiring.md`'s JST pinout table and label-swap procedure, `power.md`'s smoke-test order, `platform.md`'s SPI-contention math are all genuinely good reference material). But there is no "start here" path for a stranger. Someone landing on this repo cold hits a wall almost immediately.

### What a newcomer hits first

The README opens straight into a hardware table and architecture diagram, then a chronological "Decisions" → "Phases" log. There's no quick-start, no ordered "buy → wire → flash → boot → use" path. To actually build one, a newcomer has to reverse-engineer the sequence by reading `docs/hardware/*.md` + `docs/coex-bench.md` + `docs/web-config-spike.md` end to end — three lab notebooks totaling ~70 KB of dated bench notes.

### Priority 1 — blocks reproduction

1. **No LICENSE at repo root.** Confirmed — only vendored libs (`lib/ArduinoBLE`, `lib/Arduino_SpiNINA`, the nina-fw-airlift ArduinoBearSSL vendor tree) carry their own LICENSE. A public repo with no license is technically "all rights reserved" by default — nobody can legally fork/reuse the original code as intended. Easy fix, should be first.
2. **No BOM with purchase links.** The Hardware table (README) names parts — PyPortal Pynt #4465, simpleRTK2B Lite, HC977 antenna, PNY Elite microSD — but no links, no approximate cost, no "buy these exact SKUs." A stranger has to source everything themselves.
3. **No consolidated flash instructions.** The actual commands exist (`pio run -e pynt-bringup -t upload` in bringup-log.md §2; the coex flash procedure in coex-bench.md §7.2) but they're buried mid-narrative in bench logs, not summarized anywhere as "here are the platformio.ini environments, here's the one you actually need (`pynt-rover-coex`), here's the flash command." A table of "env → what it's for → when you'd use it" would fix this in one pass.
4. **No first-boot walkthrough.** Nothing describes what happens when you power it on for the first time: what shows on the TFT, how to join WiFi, how to find the web GUI's TRNG-generated admin password (README says it's "shown on the TFT WEB page" but that's the only mention), how to pair SW Maps over BLE vs. QField over TCP.

### Priority 2 — usability once someone pushes through P1

5. **Zero images in the repo.** Confirmed via file search — no photos or screenshots anywhere. For a project built around a hand-wired 4-conductor JST harness with a documented silkscreen label-swap gotcha ("tape-label both sockets" — wiring.md), a single wiring photo would resolve ambiguity a paragraph of prose can't. Same for the assembled device and the Phase 6 web GUI (a whole dashboard/config SPA that nobody can see without flashing it themselves).
6. **The config story is scattered across four generations and never consolidated.** Credentials/config moved: `secrets.h` (compile-time seed) → SD `/config.txt` → USB serial menu → Phase 6 web GUI (now the recommended path, per README). All four still apply in different ways but there's no single "how do I configure this thing today" doc — a reader has to track the evolution across README's Decisions section, QUESTIONS.md, and web-config-spike.md to figure out what's current.
7. **No field-use guide.** Day-2 operation — start/stop RAWX logging, flip rover↔base, read fix-quality/correction-age off the TFT — is described piecemeal in README's UI bullet points and the Phase 3 status note, never as a standalone "using it in the field" reference.

### Priority 3 — structural

8. **Docs are lab-notebook style throughout** (dated entries: "PASSED 2026-07-19," "Update 2026-07-26") rather than reference style. Great for provenance/audit trail — don't lose that — but bad for "I just need the 5 commands to flash this." Suggest splitting each of the three big narrative docs (`bringup-log.md`, `coex-bench.md`, `web-config-spike.md`) into a short **current-state summary** at the top (imperative, no dates) with the existing dated bench log kept below as history/rationale.
9. **`docs/` has no index.** No `docs/README.md` explaining which files are "read to build" (wiring.md, power.md, ucenter-config.md, checklists.md) vs. "read for history/rationale" (coex-bench.md, web-config-spike.md, QUESTIONS.md, bringup-log.md).

### Suggested README restructure

Keep everything that's there today (it's good status/history), but add a **"Build one yourself"** section near the top, above the Hardware table:

1. Full BOM with links + approximate total cost
2. Wiring summary + link to `wiring.md`, flagging the label-swap check up front
3. Flash table: env name → purpose → command (bringup validation vs. `pynt-rover-coex` as the one to actually run)
4. First-boot: TFT screens you'll see, WiFi/BLE pairing, finding the web GUI + its generated password
5. Field-use quick reference: log start/stop, rover/base switch, reading fix status
6. Add a root `LICENSE` file and a one-line `docs/README.md` index

Everything else (Hardware/Architecture/Decisions/Phases/Reference builds) stays as-is below that — it's a strong project log, just not an onboarding doc.

---

*Reviewed by the hive (QB, Fizz, Bumble, Honey) at commit `2358dfd`, 2026-08-02/03. Consolidated by QB.*
