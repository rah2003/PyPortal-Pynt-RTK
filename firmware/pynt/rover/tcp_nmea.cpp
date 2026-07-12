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

#include "features.h"
#include "gnss.h"
#include "settings.h"
#include "shared.h"

#if FEATURE_TCP_NMEA

namespace {

WiFiServer* server = nullptr;
constexpr uint8_t kMaxClients = 3;
WiFiClient clients[kMaxClients];
bool serverUp = false;

void ensureServer() {
  if (WiFi.status() != WL_CONNECTED) {
    serverUp = false;  // rebind after the next join
    return;
  }
  if (!serverUp) {
    static WiFiServer srv(g_settings.tcpPort);
    server = &srv;
    server->begin();
    serverUp = true;
    Serial.print(F("[tcp] NMEA server on :"));
    Serial.println(g_settings.tcpPort);
  }
}

void acceptClients() {
  WiFiClient incoming = server->available();
  if (!incoming) return;
  // Already tracked? (available() can return an existing client with data)
  for (uint8_t i = 0; i < kMaxClients; i++)
    if (clients[i] && clients[i] == incoming) return;
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
    return;
  }

  acceptClients();

  uint8_t live = 0;
  for (uint8_t i = 0; i < kMaxClients; i++) {
    if (!clients[i]) continue;
    if (!clients[i].connected()) {
      clients[i].stop();
      clients[i] = WiFiClient();
      continue;
    }
    while (clients[i].available()) clients[i].read();  // drain + discard
    live++;
  }
  g_link.tcpClients = live;

  char line[128];
  size_t n;
  while ((n = gnssNextNmeaLine(line, sizeof(line))) > 0) {
    if (live == 0) continue;  // still drain the queue so it can't sit stale
    for (uint8_t i = 0; i < kMaxClients; i++) {
      if (clients[i] && clients[i].connected())
        clients[i].write((const uint8_t*)line, n);
    }
  }
}

#else
void tcpNmeaInit() {}
void tcpNmeaPoll() {}
#endif  // FEATURE_TCP_NMEA
