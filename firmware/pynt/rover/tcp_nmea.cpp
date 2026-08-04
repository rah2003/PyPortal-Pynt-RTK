// TCP NMEA broadcaster for SW Maps. New module (no ancestor — Metro used
// BLE NUS here). Design notes:
//   - server starts lazily once WiFi is up (WiFiNINA servers can't bind
//     before the module has joined a network) and restarts after a drop.
//   - up to kMaxClients concurrent phones; SW Maps only listens, but
//     inbound bytes are drained and discarded so a chatty client can't
//     wedge the socket buffers.
//   - broadcast pulls whole sentences from gnss.cpp's queue; a slow or
//     stalled client gets dropped rather than back-pressuring the loop.
#include "tcp_nmea.h"

#include <WiFiNINA.h>

#include "ble_nus.h"  // FEATURE_BLE tee — calls compiled out otherwise
#include "features.h"
#include "gnss.h"
#include "settings.h"
#include "shared.h"

#if FEATURE_TCP_NMEA

namespace {

WiFiServer* server = nullptr;
constexpr uint8_t kMaxClients = 3;
WiFiClient clients[kMaxClients];
uint8_t writeFails[kMaxClients] = {0};  // consecutive short/zero writes
bool serverUp = false;

void ensureServer() {
  if (WiFi.status() != WL_CONNECTED) {
    serverUp = false;  // rebind after the next join
    return;
  }
  if (!serverUp) {
    static WiFiServer srv(g_settings.tcpPort);
    server = &srv;
#ifdef COEX_UPSTREAM_NINA
    // Team review H3: upstream WiFiServer::begin() grabs a fresh NINA
    // socket unconditionally and never frees the old one — the 10-slot
    // table dies in ~4 hotspot roams. end() first releases ours; the
    // begin(port) overload also picks up a live tcpport= change (the
    // Adafruit fork below has neither API — rebind behavior there is
    // unchanged from what passed the Phase 2/3 soaks).
    server->end();
    server->begin(g_settings.tcpPort);
    if (server->status() == 0)
      Serial.println(F("[tcp] WARN: server bind failed (socket table?)"));
#else
    server->begin();
#endif
    serverUp = true;
    Serial.print(F("[tcp] NMEA server on :"));
    Serial.println(g_settings.tcpPort);
  }
}

void acceptClients() {
#ifdef COEX_UPSTREAM_NINA
  // True accept (upstream WiFiNINA 2.x API + nina-fw 3.x): silent
  // listen-only clients (u-center; apps that don't send-on-connect) are
  // surfaced immediately. available() is STILL data-gated even on
  // nina-fw 3.0.1 — its availDataTcp only calls accept() on the
  // accept=1 path, which only server.accept() sends (soak rig finding,
  // 2026-07-27). The Adafruit fork has no accept() at all, hence the
  // gate; the Q19 data-gated behavior stands on the shipping stack.
  WiFiClient incoming = server->accept();
#else
  WiFiClient incoming = server->available();
#endif
  if (!incoming) return;
  // Already tracked? (available() can return an existing client with data.)
  // Compare remote IP:port — this fork's WiFiClient has no operator==, so
  // `clients[i] == incoming` would silently compare operator-bool results
  // and block every accept after the first.
  for (uint8_t i = 0; i < kMaxClients; i++)
    if (clients[i] && clients[i].remoteIP() == incoming.remoteIP() &&
        clients[i].remotePort() == incoming.remotePort())
      return;
  for (uint8_t i = 0; i < kMaxClients; i++) {
    if (!clients[i] || !clients[i].connected()) {
      clients[i] = incoming;
      Serial.println(F("[tcp] client connected"));
      return;
    }
  }
  incoming.stop();  // table full
}

}  // namespace

void tcpNmeaInit() {}

void tcpNmeaPoll() {
  ensureServer();
  if (!serverUp) {
    g_link.tcpClients = 0;
#if FEATURE_BLE
    // WiFi down must not stop the phone link: BLE rides this same drain
    // (the future radio scenario — corrections by UART2 radio, phone by
    // BLE — runs with no WiFi at all), and the queue still can't sit
    // stale.
    {
      char line[128];
      size_t n;
      while ((n = gnssNextNmeaLine(line, sizeof(line))) > 0)
        bleNusOnNmeaLine(line, n);
    }
#endif
    return;
  }

  acceptClients();

  uint8_t live = 0;
  for (uint8_t i = 0; i < kMaxClients; i++) {
    if (!clients[i]) continue;
    if (!clients[i].connected()) {
      clients[i].stop();
      clients[i] = WiFiClient();
      writeFails[i] = 0;
      continue;
    }
    // Bounded drain (team review M5): each read() is an SPI round-trip,
    // so an unbounded while() hands a chatty client the whole loop.
    int drainBudget = 64;
    while (drainBudget-- > 0 && clients[i].available()) clients[i].read();
    live++;
  }
  g_link.tcpClients = live;

  char line[128];
  size_t n;
  while ((n = gnssNextNmeaLine(line, sizeof(line))) > 0) {
#if FEATURE_BLE
    bleNusOnNmeaLine(line, n);  // tee: BLE runs alongside TCP, not instead
#endif
    if (live == 0) continue;  // still drain the queue so it can't sit stale
    for (uint8_t i = 0; i < kMaxClients; i++) {
      if (!clients[i] || !clients[i].connected()) continue;
      // Team review M5: the header always promised drop-on-stall; now it
      // is real. A wedged client (full NINA socket buffer) short-writes;
      // three consecutive short writes = drop it rather than let it
      // back-pressure the loop. (Bench 2026-08-02: a stalled second TCP
      // client is the leading suspect for the 8 superloop freezes.)
      if (clients[i].write((const uint8_t*)line, n) < n) {
        if (++writeFails[i] >= 3) {
          Serial.println(F("[tcp] dropping stalled client"));
          clients[i].stop();
          clients[i] = WiFiClient();
          writeFails[i] = 0;
        }
      } else {
        writeFails[i] = 0;
      }
    }
  }
}

#else
void tcpNmeaInit() {}
void tcpNmeaPoll() {}
#endif  // FEATURE_TCP_NMEA
