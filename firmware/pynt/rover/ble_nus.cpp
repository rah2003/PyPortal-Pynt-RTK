// BLE NUS phone link (FEATURE_BLE, pynt-rover-coex only). Port of Spike
// B's proven service (firmware/spike/metro_coex/): same NUS UUIDs, same
// one-sentence-per-notification framing that streamed clean for 17+ min
// concurrent with TCP on the Metro. Needs nina-fw >= 3.0.0 on the
// AirLift (BLE HCI multiplexed onto the SPI command server) + the
// upstream WiFiNINA/SpiNINA/ArduinoBLE stack.
//
// Discipline (WiFi/NTRIP stay strictly prioritized):
//   - NMEA arrives via bleNusOnNmeaLine() — the tee in tcp_nmea.cpp —
//     into a small drop-oldest ring. The tee only queues; all SPI work
//     happens in bleNusPoll(), which runs AFTER gnss/ntrip/tcp in the
//     superloop.
//   - bleNusPoll() sends at most kMaxNotifyPerPass sentences per pass,
//     and only while a central is subscribed: bounded SPI time per loop,
//     zero bus traffic when nobody listens.
//   - Sentences are never split across notifications (SW Maps parses
//     per-notification): one sentence = one 120-byte-max notify, the
//     Spike B framing. Oversize (malformed) lines drop whole.
//   - BLE.begin() failure (stock 1.7.x firmware, HCI absent) degrades to
//     a logged "off" state — the rover runs on exactly as WiFi-only.
#include "ble_nus.h"

#include "features.h"

#if FEATURE_BLE

#include <ArduinoBLE.h>

#include "settings.h"
#include "shared.h"

namespace {

// Nordic UART Service — identical UUIDs to Spike B and to what iOS
// SW Maps expects from a BLE GNSS instrument.
BLEService nus("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic nusTx("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLENotify,
                        120);
BLECharacteristic nusRx("6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
                        BLEWrite | BLEWriteWithoutResponse, 120);

// Mirrors gnss.cpp's own NMEA queue depth: one epoch's sentence burst
// fits, two epochs can't accumulate. Drop-oldest — a fresh position
// always beats a stale one on a live survey screen.
constexpr uint8_t kQueueLines = 8;
constexpr size_t kLineMax = 120;  // NMEA is <= 82 chars + CRLF; 120 = char size
constexpr uint8_t kMaxNotifyPerPass = 3;

char q[kQueueLines][kLineMax];
uint8_t qLen[kQueueLines];
uint8_t qHead = 0, qTail = 0, qCount = 0;

bool bleUp = false;

}  // namespace

void bleNusInit() {
  if (!g_settings.bleEnable) {
    Serial.println(F("[ble] disabled (bleenable=0)"));
    return;
  }
  // ntripInit() already brought up the SPI link to the NINA (SpiDrv
  // initializes inside the WiFi driver); BLE_BEGIN is just another
  // command on that link — no second bus setup, no mode switch.
  if (!BLE.begin()) {
    Serial.println(F(
        "[ble] BLE.begin() failed — nina-fw < 3.0? running WiFi-only"));
    return;
  }
  BLE.setLocalName(g_settings.bleName);  // W5 card; applies at restart
  BLE.setAdvertisedService(nus);
  nus.addCharacteristic(nusTx);
  nus.addCharacteristic(nusRx);
  BLE.addService(nus);
  BLE.advertise();
  bleUp = true;
  g_link.bleUp = true;
  Serial.print(F("[ble] advertising as "));
  Serial.print(g_settings.bleName);
  Serial.println(F(" (NUS)"));
}

void bleNusOnNmeaLine(const char* line, size_t len) {
  if (!bleUp || !g_link.bleSubscribed) return;  // queue for nobody = waste
  if (len >= kLineMax) return;  // never split a sentence; drop whole
  if (qCount == kQueueLines) {  // slow phone: drop-oldest, count it
    qTail = (qTail + 1) % kQueueLines;
    qCount--;
    g_link.bleDrops++;
  }
  memcpy(q[qHead], line, len);
  qLen[qHead] = (uint8_t)len;
  qHead = (qHead + 1) % kQueueLines;
  qCount++;
}

void bleNusPoll() {
  if (!bleUp) return;
  BLE.poll();  // service HCI events; returns immediately when idle

  bool conn = BLE.connected();
  g_link.bleConnected = conn;
  g_link.bleSubscribed = conn && nusTx.subscribed();
  if (!g_link.bleSubscribed) {
    qTail = qHead;  // flush — no stale backlog greets the next subscriber
    qCount = 0;
    return;
  }

  uint8_t sent = 0;
  while (qCount > 0 && sent < kMaxNotifyPerPass) {
    nusTx.writeValue((const uint8_t*)q[qTail], qLen[qTail]);
    qTail = (qTail + 1) % kQueueLines;
    qCount--;
    sent++;
    g_link.bleLinesTx++;
  }
}

#endif  // FEATURE_BLE
