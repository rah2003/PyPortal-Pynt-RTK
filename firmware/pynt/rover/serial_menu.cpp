// Ported from RTK-Feather's serial_menu.cpp; deltas: settings persist to
// SD (/config.txt) instead of NVS, status shows this MCU's own logger
// instead of the M0 link, and log start/stop is a menu command too (the
// touch UI is the primary control; this is the bench/recovery path).
#include "serial_menu.h"

#include "gnss.h"
#include "ntrip.h"
#include "sd_logger.h"
#include "settings.h"
#include "shared.h"

namespace {

char lineBuf[160];
size_t lineLen = 0;

void printHelp() {
  Serial.println(F("--- PyPortal-Pynt-RTK serial menu ---"));
  Serial.println(F("key=value      set + save: wifi1/pass1..wifi4/pass4, caster,"));
  Serial.println(F("               port, mount, user, password, ggaperiod,"));
  Serial.println(F("               elevmask, logubx, tcpport"));
  Serial.println(F("status         settings + live status"));
  Serial.println(F("log on|off     start/stop SD logging"));
  Serial.println(F("save           persist settings to SD /config.txt"));
  Serial.println(F("factoryrecover F9P UBX-CFG-CFG reset + reapply config"));
  Serial.println(F("help           this text"));
}

void printStatus() {
  settingsPrint(Serial);
  Serial.print(F("f9p="));
  Serial.print(g_gnss.f9pDetected ? "yes" : "no");
  Serial.print(F(" fix="));
  Serial.print(g_gnss.fixType);
  Serial.print(F(" carr="));
  Serial.print(g_gnss.carrSoln);
  Serial.print(F(" sv="));
  Serial.print(g_gnss.numSV);
  Serial.print(F(" lat="));
  Serial.print(g_gnss.latDeg, 7);
  Serial.print(F(" lon="));
  Serial.println(g_gnss.lonDeg, 7);
  Serial.print(F("ntrip="));
  Serial.print(ntripStateName());
  Serial.print(F(" wifi="));
  Serial.print(g_link.wifiUp ? "up" : "down");
  Serial.print(F(" tcpClients="));
  Serial.println(g_link.tcpClients);
  Serial.print(F("sd="));
  Serial.print(g_log.sdOk ? "ok" : "FAIL");
  Serial.print(F(" logging="));
  Serial.print(g_log.fileOpen ? "on" : "off");
  Serial.print(F(" file="));
  Serial.print(g_log.fileName[0] ? g_log.fileName : "-");
  Serial.print(F(" bytes="));
  Serial.print(g_log.bytesWritten);
  Serial.print(F(" sdFreeKB="));
  Serial.println(g_log.sdFreeKB);
}

void handleLine(char* line) {
  if (!line[0]) return;
  if (!strcmp(line, "help")) { printHelp(); return; }
  if (!strcmp(line, "status")) { printStatus(); return; }
  if (!strcmp(line, "save")) {
    Serial.println(settingsSave() ? F("saved") : F("save FAILED (no SD?)"));
    return;
  }
  if (!strcmp(line, "log on")) { sdLoggerSetEnabled(true); Serial.println(F("ok")); return; }
  if (!strcmp(line, "log off")) { sdLoggerSetEnabled(false); Serial.println(F("ok")); return; }
  if (!strcmp(line, "factoryrecover")) {
    gnssRequestFactoryRecover();
    Serial.println(F("requested"));
    return;
  }
  char* eq = strchr(line, '=');
  if (!eq) {
    Serial.println(F("unrecognized — 'help' for commands"));
    return;
  }
  *eq = '\0';
  if (settingsApplyKeyValue(line, eq + 1)) {
    Serial.println(settingsSave() ? F("ok, saved") : F("ok (NOT persisted — no SD)"));
  } else {
    Serial.println(F("unknown key — 'help' for the list"));
  }
}

}  // namespace

void serialMenuPoll() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        handleLine(lineBuf);
        lineLen = 0;
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}
