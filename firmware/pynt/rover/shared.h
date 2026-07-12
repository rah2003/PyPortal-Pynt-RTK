// Shared status state. Descended from RTK-Feather's shared.h, radically
// simplified by the single-MCU architecture: one core, one superloop —
// no cross-core ring buffers (NTRIP bytes inject directly into the GNSS
// library in ntrip.cpp), no mutexes (writers and readers are the same
// thread), no M0Status (there is no second MCU; the logger reports
// through LogStatus below).
#pragma once
#include <Arduino.h>

struct GnssStatus {
  bool f9pDetected = false;
  uint8_t fixType = 0;   // NAV-PVT fixType: 0 none, 2 2D, 3 3D
  uint8_t carrSoln = 0;  // 0 none, 1 float, 2 fixed
  uint8_t numSV = 0;
  double latDeg = 0, lonDeg = 0;
  double hMslM = 0;
  uint32_t hAccMm = 0, vAccMm = 0;
  float pdop = 0;
  bool timeValid = false;
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
  char lastGga[120] = {0};  // most recent GGA sentence (NTRIP upstream)
  uint32_t lastGgaMs = 0;
};

struct LinkStatus {
  bool wifiUp = false;
  bool ntripConnected = false;
  uint32_t lastRtcmMs = 0;
  uint32_t rtcmBytes = 0;
  int8_t wifiRssi = 0;   // dBm; 0 = unknown
  uint8_t tcpClients = 0;  // SW Maps connections (tcp_nmea.cpp)
};

struct LogStatus {
  bool sdOk = false;
  bool loggingEnabled = true;  // "always-on" default (Metro Q6 pattern)
  bool fileOpen = false;
  char fileName[40] = {0};
  uint32_t bytesWritten = 0;
  uint32_t sdFreeKB = 0;
  uint32_t bufHighWater = 0;  // GNSS file-buffer high-water (platform.md math check)
};

struct BaseStatus {  // NAV-SVIN, only meaningful in Base mode
  bool svinActive = false;
  bool svinValid = false;
  uint32_t svinDurS = 0;
  uint32_t svinMeanAcc01mm = 0;  // 0.1 mm units, as reported
};

extern GnssStatus g_gnss;  // written by gnss.cpp only
extern LinkStatus g_link;  // written by ntrip.cpp / tcp_nmea.cpp only
extern LogStatus g_log;    // written by sd_logger.cpp only
extern BaseStatus g_base;  // written by gnss.cpp only

// Correction age in ms (UINT32_MAX if never received).
uint32_t correctionAgeMs();
