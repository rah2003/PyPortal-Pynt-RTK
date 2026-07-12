// Device configuration. Descended from RTK-Feather's settings.h; the
// persistence mechanism changes with the hardware: the Pynt has its own
// microSD, so the Metro "SD /config.txt" mechanism (which the Feather
// couldn't host) comes back — /config.txt on the card is the store, with
// an optional compiled-in secrets.h seed for bench work. No locks: the
// single superloop means the serial menu and every reader share a thread.
#pragma once
#include <Arduino.h>

constexpr int kMaxWifiNetworks = 4;

enum class DeviceMode : uint8_t { Rover = 0, Base = 1 };

struct Settings {
  // --- WiFi (tried in order) ---
  char wifiSsid[kMaxWifiNetworks][33] = {{0}};
  char wifiPass[kMaxWifiNetworks][65] = {{0}};

  // --- NTRIP caster --- defaults inherited from Metro via Feather.
  char casterHost[65] = "acorn-gnss.net";
  uint16_t casterPort = 2101;
  char casterMount[49] = "VRS_SouthCentral_RTCM3";  // alt: MS_RTCM3
  char casterUser[49] = {0};  // never defaulted — see secrets.example.h
  char casterPass[49] = {0};
  uint16_t ggaPeriodS = 10;

  // --- Mode --- Rover in Phase 2; Base arrives with Phase 3.
  DeviceMode mode = DeviceMode::Rover;

  // --- GNSS ---
  uint8_t elevMaskDeg = 12;  // inherited Metro answer

  // --- Logging / phone link ---
  bool logUbx = true;       // log from boot; the LOG touch button toggles
  uint16_t tcpPort = 10110; // SW Maps NMEA server port
};

extern Settings g_settings;

void settingsLoad();   // SD /config.txt -> g_settings (secrets.h seeds bench defaults)
bool settingsSave();   // g_settings -> SD /config.txt (false if no card)
void settingsPrint(Stream& out);  // passwords masked
bool settingsApplyKeyValue(const char* key, const char* value);
