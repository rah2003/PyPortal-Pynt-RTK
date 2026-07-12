// NTRIP client state machine. Ported from RTK-Feather's ntrip_client.cpp
// (itself near-verbatim Metro), re-plumbed for this platform:
//   - WiFiNINA (AirLift co-processor) instead of the ESP32's native WiFi:
//     no WiFi.mode/setSleep, no per-connect timeout arg, RSSI via the
//     same call. WiFi.setPins happens once in ntripInit().
//   - polled state machine instead of a FreeRTOS task: header parsing is
//     its own state with a millis() deadline instead of a blocking loop.
//   - no rtcmRing: single core — bytes go straight to gnssInjectRtcm().
//   - no esp_task_wdt: SAMD51 superloop; a wedged socket is bounded by
//     the same state deadlines that replaced the blocking reads.
//
// KNOWN BOUNDED STALL: WiFiNINA's WiFi.begin() blocks inside the NINA
// co-processor for up to a few seconds per attempt. During that window
// SerialGNSS is unserviced; SERIAL_BUFFER_SIZE is raised to 4096 in
// platformio.ini to ride it out, and a WiFi (re)join costs at most a few
// RAWX epochs — bounded, rare (hotspot drop), accepted for v1.
//
// Kept verbatim from the ancestors: rev1 "ICY 200 OK" + rev2 "HTTP/1.x
// 200" acceptance, SOURCETABLE = wrong mountpoint, exponential backoff
// 1 s -> 60 s, >10 s correction-stale teardown, GGA upstream cadence,
// unsigned-widened base64 (signed char sign-extension = permanent 401).
#include "ntrip.h"

#include <WiFiNINA.h>

#include "features.h"
#include "gnss.h"
#include "pins.h"
#include "settings.h"
#include "shared.h"

#if FEATURE_NTRIP

namespace {

enum class NtripState {
  WifiConnecting,
  CasterConnecting,
  HeaderWait,
  Connected,
  WaitRetry
};
NtripState state = NtripState::WifiConnecting;

WiFiClient sock;
uint32_t retryDelayMs = 1000;
constexpr uint32_t kRetryMaxMs = 60000;
constexpr uint32_t kSocketTimeoutMs = 5000;
constexpr uint32_t kCorrectionStaleMs = 10000;
uint32_t stateEnteredMs = 0;
uint32_t lastGgaSentMs = 0;
uint8_t wifiIdx = 0;  // which configured SSID we try next

// Header-parse scratch (HeaderWait state)
char hdrLine[256];
size_t hdrLen = 0;
bool hdrOk = false;

const char* kStateNames[] = {"wifi-connecting", "caster-connecting",
                             "header-wait", "connected", "wait-retry"};

void enter(NtripState s) {
  state = s;
  stateEnteredMs = millis();
}

void backoff() {
  sock.stop();
  g_link.ntripConnected = false;
  enter(NtripState::WaitRetry);
}

void base64enc(const char* in, char* out, size_t outLen) {
  static const char tbl[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t n = strlen(in), o = 0;
  for (size_t i = 0; i < n && o + 4 < outLen; i += 3) {
    uint32_t v = (uint32_t)(uint8_t)in[i] << 16 |
                 (i + 1 < n ? (uint32_t)(uint8_t)in[i + 1] << 8 : 0) |
                 (i + 2 < n ? (uint32_t)(uint8_t)in[i + 2] : 0);
    out[o++] = tbl[(v >> 18) & 63];
    out[o++] = tbl[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? tbl[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? tbl[v & 63] : '=';
  }
  out[o] = '\0';
}

// One SSID attempt per poll pass — WiFi.begin() itself is the bounded
// stall documented above; cycling one network per pass keeps the UI and
// GNSS serviced between attempts.
bool wifiTryNext() {
  for (int attempts = 0; attempts < kMaxWifiNetworks; attempts++) {
    uint8_t i = wifiIdx;
    wifiIdx = (wifiIdx + 1) % kMaxWifiNetworks;
    if (!g_settings.wifiSsid[i][0]) continue;
    Serial.print(F("[ntrip] WiFi: trying "));
    Serial.println(g_settings.wifiSsid[i]);
    if (WiFi.begin(g_settings.wifiSsid[i], g_settings.wifiPass[i]) ==
        WL_CONNECTED) {
      Serial.print(F("[ntrip] WiFi up: "));
      Serial.print(WiFi.localIP());
      Serial.print(F("  RSSI "));
      Serial.println(WiFi.RSSI());
      return true;
    }
    return false;  // one attempt per pass; next pass tries the next SSID
  }
  return false;  // nothing configured
}

bool casterSendRequest() {
  const char* host = g_settings.casterHost;
  const char* mount = g_settings.casterMount;
  if (!host[0] || !mount[0]) {
    Serial.println(F("[ntrip] no caster configured (serial menu or secrets.h)"));
    return false;
  }
  if (!sock.connect(host, g_settings.casterPort)) {
    Serial.print(F("[ntrip] TCP connect to "));
    Serial.print(host);
    Serial.print(':');
    Serial.print(g_settings.casterPort);
    Serial.println(F(" failed"));
    return false;
  }

  char auth[100] = {0}, authB64[140] = {0};
  if (g_settings.casterUser[0]) {
    snprintf(auth, sizeof(auth), "%s:%s", g_settings.casterUser,
             g_settings.casterPass);
    base64enc(auth, authB64, sizeof(authB64));
  }

  char req[512];
  int n = snprintf(req, sizeof(req),
                   "GET /%s HTTP/1.1\r\n"
                   "Host: %s:%u\r\n"
                   "Ntrip-Version: Ntrip/2.0\r\n"
                   "User-Agent: NTRIP PyPortalPyntRTK/1.0\r\n"
                   "Accept: */*\r\n"
                   "Connection: close\r\n",
                   mount, host, g_settings.casterPort);
  if (authB64[0])
    n += snprintf(req + n, sizeof(req) - n, "Authorization: Basic %s\r\n",
                  authB64);
  n += snprintf(req + n, sizeof(req) - n, "\r\n");
  sock.write((const uint8_t*)req, n);
  hdrLen = 0;
  hdrOk = false;
  return true;
}

// Non-blocking header consumption; returns true when headers are done
// (hdrOk says whether the mount answered 200). rev1 "ICY 200 OK" ends
// header parsing immediately — the next buffered bytes are binary RTCM.
bool headerPoll() {
  while (sock.available()) {
    char c = (char)sock.read();
    if (c == '\n') {
      hdrLine[hdrLen] = '\0';
      if (hdrLen <= 1) return true;  // blank line = end of headers (rev2)
      if (strstr(hdrLine, "ICY 200 OK")) {
        hdrOk = true;
        return true;  // rev1: RTCM starts NOW — stop consuming
      }
      if (strstr(hdrLine, "HTTP/1.1 200") || strstr(hdrLine, "HTTP/1.0 200"))
        hdrOk = true;  // rev2: keep reading to the blank line
      if (strstr(hdrLine, "SOURCETABLE")) {
        Serial.print(F("[ntrip] mountpoint '"));
        Serial.print(g_settings.casterMount);
        Serial.println(F("' not found (got SOURCETABLE)"));
        hdrOk = false;
        return true;
      }
      if (strstr(hdrLine, "401") || strstr(hdrLine, "403"))
        Serial.println(F("[ntrip] caster rejected credentials"));
      hdrLen = 0;
    } else if (c != '\r' && hdrLen < sizeof(hdrLine) - 1) {
      hdrLine[hdrLen++] = c;
    }
  }
  return false;  // headers still in flight
}

void maybeSendGga() {
  if (millis() - lastGgaSentMs < (uint32_t)g_settings.ggaPeriodS * 1000)
    return;
  if (!g_gnss.lastGga[0]) return;
  sock.print(g_gnss.lastGga);  // sentence includes \r\n
  lastGgaSentMs = millis();
}

}  // namespace

void ntripInit() {
  WiFi.setPins(SPIWIFI_SS, SPIWIFI_ACK, ESP32_RESETN, ESP32_GPIO0, &SPIWIFI);
  if (WiFi.status() == WL_NO_MODULE)
    Serial.println(F("[ntrip] AirLift not responding — check SPIWIFI pins"));
  enter(NtripState::WifiConnecting);
}

void ntripPoll() {
  g_link.wifiUp = (WiFi.status() == WL_CONNECTED);
  g_link.wifiRssi = g_link.wifiUp ? (int8_t)WiFi.RSSI() : 0;

  switch (state) {
    case NtripState::WifiConnecting:
      if (wifiTryNext()) enter(NtripState::CasterConnecting);
      else enter(NtripState::WaitRetry);
      break;

    case NtripState::CasterConnecting:
      if (WiFi.status() != WL_CONNECTED) {
        enter(NtripState::WifiConnecting);
        break;
      }
      if (casterSendRequest()) enter(NtripState::HeaderWait);
      else backoff();
      break;

    case NtripState::HeaderWait:
      if (!sock.connected() ||
          millis() - stateEnteredMs > kSocketTimeoutMs) {
        Serial.println(F("[ntrip] caster response timeout"));
        backoff();
        break;
      }
      if (headerPoll()) {
        if (hdrOk) {
          Serial.println(F("[ntrip] caster connected, RTCM flowing"));
          g_link.ntripConnected = true;
          g_link.lastRtcmMs = millis();  // grace before the stale watchdog
          retryDelayMs = 1000;
          lastGgaSentMs = 0;
          enter(NtripState::Connected);
        } else {
          backoff();
        }
      }
      break;

    case NtripState::Connected: {
      if (WiFi.status() != WL_CONNECTED || !sock.connected()) {
        Serial.println(F("[ntrip] link lost"));
        backoff();
        break;
      }
      uint8_t buf[512];
      while (sock.available()) {
        int n = sock.read(buf, sizeof(buf));
        if (n <= 0) break;
        gnssInjectRtcm(buf, (size_t)n);
        g_link.lastRtcmMs = millis();
        g_link.rtcmBytes += n;
      }
      if (millis() - g_link.lastRtcmMs > kCorrectionStaleMs) {
        // Half-open TCP is common on hotspot handoffs.
        Serial.println(F("[ntrip] corrections stale >10 s, reconnecting"));
        backoff();
        break;
      }
      maybeSendGga();
      break;
    }

    case NtripState::WaitRetry:
      if (millis() - stateEnteredMs >= retryDelayMs) {
        retryDelayMs = min(retryDelayMs * 2, kRetryMaxMs);
        enter(WiFi.status() == WL_CONNECTED ? NtripState::CasterConnecting
                                            : NtripState::WifiConnecting);
      }
      break;
  }
}

const char* ntripStateName() { return kStateNames[(int)state]; }

#else
void ntripInit() {}
void ntripPoll() {}
const char* ntripStateName() { return "disabled"; }
#endif  // FEATURE_NTRIP
