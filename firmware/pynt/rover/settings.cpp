// SD-backed key=value config. File format is the serial menu's own
// key=value lines, one per line, '#' comments allowed — so /config.txt
// can be edited on a laptop with any text editor and the menu and file
// stay one vocabulary.
#include "settings.h"

#include <SdFat.h>

#include "sd_logger.h"  // sdCard() + sdIsOk()

#if __has_include("secrets.h")
#include "secrets.h"
#define HAVE_SECRETS 1
#else
#define HAVE_SECRETS 0
#endif

Settings g_settings;

namespace {
constexpr const char* kConfigPath = "/config.txt";

void seedFromSecrets() {
#if HAVE_SECRETS
#ifdef SECRET_WIFI_SSID1
  strncpy(g_settings.wifiSsid[0], SECRET_WIFI_SSID1, sizeof(g_settings.wifiSsid[0]) - 1);
  strncpy(g_settings.wifiPass[0], SECRET_WIFI_PASS1, sizeof(g_settings.wifiPass[0]) - 1);
#endif
#ifdef SECRET_CASTER_USER
  strncpy(g_settings.casterUser, SECRET_CASTER_USER, sizeof(g_settings.casterUser) - 1);
  strncpy(g_settings.casterPass, SECRET_CASTER_PASS, sizeof(g_settings.casterPass) - 1);
#endif
#endif
}

void applyLine(char* line) {
  // Trim CR and leading spaces; skip comments/blanks.
  while (*line == ' ' || *line == '\t') line++;
  char* cr = strchr(line, '\r');
  if (cr) *cr = '\0';
  if (!line[0] || line[0] == '#') return;
  char* eq = strchr(line, '=');
  if (!eq) return;
  *eq = '\0';
  settingsApplyKeyValue(line, eq + 1);
}
}  // namespace

void settingsLoad() {
  seedFromSecrets();  // compiled-in bench defaults first; file overrides
  if (!sdIsOk()) {
    Serial.println(F("[cfg] no SD — running on defaults/secrets"));
    return;
  }
  FsFile f = sdCard().open(kConfigPath, O_RDONLY);
  if (!f) {
    Serial.println(F("[cfg] no /config.txt — running on defaults/secrets"));
    return;
  }
  char line[160];
  size_t n = 0;
  while (f.available()) {
    char c = (char)f.read();
    if (c == '\n') {
      line[n] = '\0';
      applyLine(line);
      n = 0;
    } else if (n < sizeof(line) - 1) {
      line[n++] = c;
    }
  }
  if (n) { line[n] = '\0'; applyLine(line); }
  f.close();
  Serial.println(F("[cfg] loaded /config.txt"));
}

bool settingsSave() {
  if (!sdIsOk()) {
    Serial.println(F("[cfg] no SD — cannot persist (settings live until reboot)"));
    return false;
  }
  FsFile f = sdCard().open(kConfigPath, O_WRONLY | O_CREAT | O_TRUNC);
  if (!f) return false;
  f.println(F("# PyPortal-Pynt-RTK config — key=value, same keys as the serial menu"));
  for (int i = 0; i < kMaxWifiNetworks; i++) {
    if (!g_settings.wifiSsid[i][0]) continue;
    f.print(F("wifi")); f.print(i + 1); f.print('=');
    f.println(g_settings.wifiSsid[i]);
    f.print(F("pass")); f.print(i + 1); f.print('=');
    f.println(g_settings.wifiPass[i]);
  }
  f.print(F("caster="));    f.println(g_settings.casterHost);
  f.print(F("port="));      f.println(g_settings.casterPort);
  f.print(F("mount="));     f.println(g_settings.casterMount);
  f.print(F("user="));      f.println(g_settings.casterUser);
  f.print(F("password="));  f.println(g_settings.casterPass);
  f.print(F("ggaperiod=")); f.println(g_settings.ggaPeriodS);
  f.print(F("elevmask="));  f.println(g_settings.elevMaskDeg);
  f.print(F("logubx="));    f.println(g_settings.logUbx ? 1 : 0);
  f.print(F("tcpport="));   f.println(g_settings.tcpPort);
  f.close();
  return true;
}

void settingsPrint(Stream& out) {
  for (int i = 0; i < kMaxWifiNetworks; i++) {
    if (!g_settings.wifiSsid[i][0]) continue;
    out.print(F("wifi")); out.print(i + 1); out.print(F("="));
    out.print(g_settings.wifiSsid[i]); out.println(F("  pass=****"));
  }
  out.print(F("caster=")); out.print(g_settings.casterHost);
  out.print(F(":")); out.print(g_settings.casterPort);
  out.print(F("/")); out.println(g_settings.casterMount);
  out.print(F("user=")); out.print(g_settings.casterUser[0] ? "set" : "unset");
  out.print(F(" password=")); out.println(g_settings.casterPass[0] ? "set" : "unset");
  out.print(F("ggaperiod=")); out.println(g_settings.ggaPeriodS);
  out.print(F("elevmask=")); out.println(g_settings.elevMaskDeg);
  out.print(F("logubx=")); out.println(g_settings.logUbx ? 1 : 0);
  out.print(F("tcpport=")); out.println(g_settings.tcpPort);
}

bool settingsApplyKeyValue(const char* key, const char* value) {
  auto setStr = [](char* dst, size_t cap, const char* v) {
    strncpy(dst, v, cap - 1);
    dst[cap - 1] = '\0';
  };
  for (int i = 0; i < kMaxWifiNetworks; i++) {
    char k[8];
    snprintf(k, sizeof(k), "wifi%d", i + 1);
    if (!strcmp(key, k)) { setStr(g_settings.wifiSsid[i], 33, value); return true; }
    snprintf(k, sizeof(k), "pass%d", i + 1);
    if (!strcmp(key, k)) { setStr(g_settings.wifiPass[i], 65, value); return true; }
  }
  if (!strcmp(key, "caster"))    { setStr(g_settings.casterHost, 65, value); return true; }
  if (!strcmp(key, "port"))      { g_settings.casterPort = (uint16_t)atoi(value); return true; }
  if (!strcmp(key, "mount"))     { setStr(g_settings.casterMount, 49, value); return true; }
  if (!strcmp(key, "user"))      { setStr(g_settings.casterUser, 49, value); return true; }
  if (!strcmp(key, "password"))  { setStr(g_settings.casterPass, 49, value); return true; }
  if (!strcmp(key, "ggaperiod")) { g_settings.ggaPeriodS = (uint16_t)atoi(value); return true; }
  if (!strcmp(key, "elevmask"))  { g_settings.elevMaskDeg = (uint8_t)atoi(value); return true; }
  if (!strcmp(key, "logubx"))    { g_settings.logUbx = atoi(value) != 0; return true; }
  if (!strcmp(key, "tcpport"))   { g_settings.tcpPort = (uint16_t)atoi(value); return true; }
  return false;
}
