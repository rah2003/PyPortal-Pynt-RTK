// W0 spike — answers the web-config unknowns on spare hardware (Metro
// M4 AirLift with nina-fw 3.0.1-airlift) BEFORE any of it runs on the
// Pynt. Procedure + pass criteria: docs/web-config-spike.md.
//
// Serial menu:
//   1  STA mode: join hotspot, HTTP server on :80 (gzipped page from
//      PROGMEM + /api/ping JSON + /big 16 KiB throughput payload)
//   2  AP mode: WiFi.beginAP("PyntRTK-setup", pass) + same HTTP server
//   b  BLE.begin() + advertise "PyntRTK-web-spike" (run in mode 1 or 2
//      to answer STA+BLE and AP+BLE coexistence)
//   h  WiFi.setHostname("pynt-w0-spike") then (re)join — check the
//      router's DHCP table afterwards
//   s  status line (mode, IP, clients served, BLE state)
#include <Arduino.h>
#include <SPI.h>
#include <ArduinoBLE.h>
#include <WiFiNINA.h>

#if __has_include("../secrets.h")
#include "../secrets.h"
#else
#define SPIKE_WIFI_SSID "set-me: firmware/spike/secrets.h"
#define SPIKE_WIFI_PASS ""
#endif

#include "../../pynt/rover/web_assets.h"  // same pipeline the rover uses

namespace {

WiFiServer server(80);
bool serverUp = false;
uint8_t netMode = 0;  // 0 none, 1 STA, 2 AP
bool bleUp = false;
uint32_t served = 0;

uint8_t bigChunk[512];  // /big streams 32 x 512 B = 16 KiB

void httpServe() {
  WiFiClient c = server.accept();
  if (!c) return;
  // Spike-only: blocking per-request handling (bounded by timeouts) —
  // the rover module is the non-blocking version of this.
  uint32_t t0 = millis();
  char line[128];
  size_t n = 0;
  bool gotLine = false;
  while (c.connected() && millis() - t0 < 2000) {
    if (!c.available()) continue;
    char ch = (char)c.read();
    if (ch == '\n') {
      if (!gotLine) { line[n] = '\0'; gotLine = true; }
      if (n == 0 && gotLine) break;  // crude: first blank-ish end
      n = 0;
    } else if (ch != '\r' && n < sizeof(line) - 1) {
      line[n++] = ch;
      if (!gotLine) continue;
    }
  }
  line[sizeof(line) - 1] = '\0';

  if (strstr(line, "GET /big")) {
    c.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
              "Content-Length: 16384\r\nConnection: close\r\n\r\n"));
    uint32_t w0 = millis();
    for (int i = 0; i < 32; i++) c.write(bigChunk, sizeof(bigChunk));
    c.flush();
    Serial.print(F("[http] /big 16384 B in "));
    Serial.print(millis() - w0);
    Serial.println(F(" ms"));
  } else if (strstr(line, "GET /api/ping")) {
    char body[80];
    int bn = snprintf(body, sizeof(body),
                      "{\"up\":%lu,\"served\":%lu,\"ble\":%d}",
                      (unsigned long)(millis() / 1000),
                      (unsigned long)served, bleUp ? 1 : 0);
    c.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
              "Connection: close\r\nContent-Length: "));
    c.print(bn);
    c.print(F("\r\n\r\n"));
    c.print(body);
  } else {
    c.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
              "Content-Encoding: gzip\r\nConnection: close\r\n"
              "Content-Length: "));
    c.print((unsigned)WEB_INDEX_GZ_LEN);
    c.print(F("\r\n\r\n"));
    c.write(WEB_INDEX_GZ, WEB_INDEX_GZ_LEN);
  }
  delay(5);
  c.stop();
  served++;
}

void printStatus() {
  Serial.print(F("mode="));
  Serial.print(netMode == 1 ? F("STA") : netMode == 2 ? F("AP") : F("none"));
  Serial.print(F(" wifiStatus="));
  Serial.print(WiFi.status());
  Serial.print(F(" ip="));
  Serial.print(WiFi.localIP());
  Serial.print(F(" served="));
  Serial.print(served);
  Serial.print(F(" ble="));
  Serial.println(bleUp ? (BLE.connected() ? F("connected") : F("advertising"))
                       : F("off"));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  Serial.println();
  Serial.println(F("=== W0 web spike (docs/web-config-spike.md) ==="));
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("[FAIL] module not responding"));
    while (true) delay(1000);
  }
  Serial.print(F("NINA firmware: "));
  Serial.println(WiFi.firmwareVersion());
  for (size_t i = 0; i < sizeof(bigChunk); i++) bigChunk[i] = (uint8_t)i;
  Serial.println(F("menu: 1=STA+HTTP 2=AP+HTTP b=BLE h=hostname s=status"));
}

void loop() {
  if (serverUp) httpServe();
  if (bleUp) BLE.poll();

  if (!Serial.available()) return;
  char cmd = (char)Serial.read();
  while (Serial.available()) Serial.read();
  switch (cmd) {
    case '1': {
      if (netMode) WiFi.end();  // end() only when something was begun —
                                // an unconditional end() before the first
                                // begin() broke every join (bench find)
      netMode = 0;
      serverUp = false;
      Serial.print(F("joining "));
      Serial.println(F(SPIKE_WIFI_SSID));
      if (WiFi.begin(SPIKE_WIFI_SSID, SPIKE_WIFI_PASS) == WL_CONNECTED) {
        netMode = 1;
        server.begin();
        serverUp = true;
        Serial.print(F("STA up — http://"));
        Serial.print(WiFi.localIP());
        Serial.println(F("/"));
      } else {
        Serial.println(F("[FAIL] join"));
      }
      break;
    }
    case '2': {
      if (netMode) WiFi.end();
      netMode = 0;
      serverUp = false;
      // WPA2 AP — W0.5. Password fixed for the spike; the rover
      // generates its own (TFT-displayed) key.
      uint8_t r = WiFi.beginAP("PyntRTK-setup", "pynt-spike-99");
      Serial.print(F("beginAP -> "));
      Serial.println(r);
      if (r == WL_AP_LISTENING) {
        netMode = 2;
        server.begin();
        serverUp = true;
        Serial.print(F("AP up — join it, then http://"));
        Serial.print(WiFi.localIP());
        Serial.println(F("/"));
      } else {
        Serial.println(F("[FAIL] beginAP (record the code — W2 design input)"));
      }
      break;
    }
    case 'b':
      if (!bleUp) {
        if (BLE.begin()) {
          BLE.setLocalName("PyntRTK-web-spike");
          BLE.advertise();
          bleUp = true;
          Serial.println(F("[ble] advertising — check phone NOW (coex answer)"));
        } else {
          Serial.println(F("[ble] begin FAILED in this mode — record it! "
                           "(W2 design input)"));
        }
      } else {
        BLE.end();
        bleUp = false;
        Serial.println(F("[ble] off"));
      }
      break;
    case 'h':
      WiFi.setHostname("pynt-w0-spike");
      Serial.println(F("hostname set — rejoin with '1', then check the "
                       "router's DHCP client table"));
      break;
    case 's':
      printStatus();
      break;
    default:
      Serial.println(F("menu: 1=STA+HTTP 2=AP+HTTP b=BLE h=hostname s=status"));
  }
}
