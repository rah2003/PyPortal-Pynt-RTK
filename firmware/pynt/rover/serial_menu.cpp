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
#include "wdt.h"

namespace {

// Free RAM between heap top and current stack — the §7.5 memory-stability
// gate was unmeasurable without this (soak 2026-08-02).
extern "C" char* sbrk(int incr);
int freeRamBytes() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}

char lineBuf[160];
size_t lineLen = 0;

void printHelp() {
  Serial.println(F("--- PyPortal-Pynt-RTK serial menu ---"));
  Serial.println(F("key=value      set + save: wifi1/pass1..wifi4/pass4, caster,"));
  Serial.println(F("               port, mount, user, password, ggaperiod,"));
  Serial.println(F("               elevmask, logubx, tcpport"));
  Serial.println(F("               mode=rover|base, svindur (s), svinacc (0.1mm),"));
  Serial.println(F("               fixedlat/fixedlon (deg), fixedalt (m)"));
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
  Serial.print(F("mode="));
  Serial.print(g_settings.mode == DeviceMode::Base ? "base" : "rover");
  if (g_settings.mode == DeviceMode::Base) {
    Serial.print(F(" svin: active="));
    Serial.print(g_base.svinActive);
    Serial.print(F(" valid="));
    Serial.print(g_base.svinValid);
    Serial.print(F(" dur="));
    Serial.print(g_base.svinDurS);
    Serial.print(F("s meanAcc="));
    Serial.print(g_base.svinMeanAcc01mm / 10000.0f, 2);
    Serial.print(F("m"));
  }
  Serial.println();
  // Phase C receiver-truth telemetry: corrections as the F9P sees them
  // (not link liveness), baseline to the (virtual) base, C/N0, RF health.
  {
    uint32_t rxAge = corrRxAgeMs();
    Serial.print(F("corr: used="));
    Serial.print(g_corr.corUsed);
    Serial.print('/');
    Serial.print(g_corr.corMsgs);
    Serial.print(F(" err="));
    Serial.print(g_corr.corErrors);
    Serial.print(F(" rxAge="));
    if (rxAge == UINT32_MAX) Serial.print('-');
    else Serial.print(rxAge / 1000);
    Serial.print(F("s ggaAge="));
    Serial.print(g_corr.ggaDiffAgeS, 0);
    Serial.print(F(" type="));
    Serial.print(g_corr.lastMsgType);
    Serial.print(F(" base="));
    Serial.print(g_corr.baselineM, 0);
    Serial.print(F("m ref="));
    Serial.println(g_corr.refStationId);
    Serial.print(F("rf: sats="));
    Serial.print(g_corr.satsUsed);
    Serial.print(F(" cn0="));
    Serial.print(g_corr.meanCn0, 1);
    Serial.print('/');
    Serial.print(g_corr.maxCn0);
    for (uint8_t b = 0; b < g_corr.rfBlocks; b++) {
      Serial.print(b == 0 ? F(" L1[") : F(" L2["));
      Serial.print(F("jam="));
      Serial.print(g_corr.jammingState[b]);
      Serial.print('/');
      Serial.print(g_corr.jamInd[b]);
      Serial.print(F(" agc="));
      Serial.print(g_corr.agcPct[b]);
      Serial.print(F("% noise="));
      Serial.print(g_corr.noisePerMS[b]);
      Serial.print(']');
    }
    Serial.println();
  }
  Serial.print(F("ntrip="));
  Serial.print(ntripStateName());
  Serial.print(F(" wifi="));
  Serial.print(g_link.wifiUp ? "up" : "down");
  if (g_link.wifiUp) {
    Serial.print(F(" ip="));
    Serial.print(g_link.wifiIp);
    Serial.print(F(":"));
    Serial.print(g_settings.tcpPort);
  }
  Serial.print(F(" tcpClients="));
  Serial.println(g_link.tcpClients);
#if FEATURE_BLE
  Serial.print(F("ble="));
  Serial.print(!g_link.bleUp        ? "off"
               : g_link.bleSubscribed ? "streaming"
               : g_link.bleConnected  ? "connected"
                                      : "advertising");
  Serial.print(F(" linesTx="));
  Serial.print(g_link.bleLinesTx);
  Serial.print(F(" bleDrops="));
  Serial.println(g_link.bleDrops);
#endif
  Serial.print(F("freeRam="));
  Serial.print(freeRamBytes());
  if (wdtLastFreeze()[0]) {
    Serial.print(F(" lastWdtFreeze="));
    Serial.print(wdtLastFreeze());
  }
  Serial.println();
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
    // Mode and base-position keys change the F9P's TMODE config —
    // re-apply live rather than requiring a reboot.
    if (!strcmp(line, "mode") || !strncmp(line, "svin", 4) ||
        !strncmp(line, "fixed", 5)) {
      gnssRequestModeApply();
      // The r_/b_ log prefix is chosen at file open — cycle the logger
      // on a live mode switch so the new mode gets its own file.
      if (!strcmp(line, "mode") && g_log.fileOpen) {
        sdLoggerSetEnabled(false);
        sdLoggerSetEnabled(true);
      }
    }
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
