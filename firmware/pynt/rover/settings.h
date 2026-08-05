// Device configuration. Descended from RTK-Feather's settings.h; the
// persistence mechanism changes with the hardware: the Pynt has its own
// microSD, so the Metro "SD /config.txt" mechanism (which the Feather
// couldn't host) comes back — /config.txt on the card is the store, with
// an optional compiled-in secrets.h seed for bench work. No locks: the
// single superloop means the serial menu and every reader share a thread.
#pragma once
#include <Arduino.h>

#include "features.h"  // ENABLE_WEB_CONFIG / FEATURE_BLE gate fields below

constexpr int kMaxWifiNetworks = 4;

enum class DeviceMode : uint8_t { Rover = 0, Base = 1 };

struct Settings {
  // --- WiFi (tried in order) ---
  char wifiSsid[kMaxWifiNetworks][33] = {{0}};
  char wifiPass[kMaxWifiNetworks][65] = {{0}};

  // --- NTRIP caster --- defaults inherited from Metro via Feather.
  // 2026-07-19: the bare apex lost its DNS A record (provider change) —
  // only www. resolves now; verified listening on :2101 before switching.
  char casterHost[65] = "www.acorn-gnss.net";
  uint16_t casterPort = 2101;
  char casterMount[49] = "VRS_SouthCentral_RTCM3";  // alt: MS_RTCM3
  char casterUser[49] = {0};  // never defaulted — see secrets.example.h
  char casterPass[49] = {0};
  uint16_t ggaPeriodS = 10;

  // --- Mode ---
  DeviceMode mode = DeviceMode::Rover;

  // --- Base mode (Phase 3) --- survey-in by default; a nonzero
  // fixedLat/fixedLon switches to fixed-position TMODE.
  uint16_t svinMinDurS = 300;        // CFG-TMODE-SVIN_MIN_DUR
  uint32_t svinAccLimit01mm = 50000; // CFG-TMODE-SVIN_ACC_LIMIT, 0.1 mm units (50000 = 5 m)
  double fixedLatDeg = 0;            // 0 = use survey-in
  double fixedLonDeg = 0;
  double fixedAltM = 0;              // ellipsoidal height for CFG-TMODE-HEIGHT

  // --- GNSS ---
  uint8_t elevMaskDeg = 12;  // inherited Metro answer

  // --- Datum/epoch METADATA (team review P1, Phase E prep) ---
  // The frame the VRS corrections put the rover in — a label carried to
  // status surfaces and the field log, NOT a transform. Operator answer
  // 2026-08-05: VRS_SouthCentral_RTCM3 broadcasts NAD83(2011) epoch
  // 2010.00 (ACORN holds CORS positions fixed to NSRS 2010.00 against
  // SC-Alaska tectonic motion). Mislabeling this as WGS84/ITRF is a
  // silent ~1-2 m systematic error — docs/datum-epoch.md.
  char datum[20] = "NAD83(2011)";
  char epoch[12] = "2010.00";

  // Survey pole height in meters: ground mark -> ARP, where the ARP is
  // the ENCLOSURE BOTTOM FACE (owner spec 2026-08-05). Fixed offsets
  // above the ARP (antenna coaxial with pole, 0 deg tilt, ~0 lateral):
  // enclosure->antenna seat 106.3 mm; ARP->L1 PC 138.3 mm; ARP->L2/L5
  // PC 143.3 mm (each +/-3 mm, HC977 datasheet PCO + seat height);
  // iono-free effective ARP->PC 130.6 mm. Metadata only — recorded to
  // the field log / RINEX, never applied to positions. 0 = unset.
  float antHeightM = 0;

  // --- Logging / phone link ---
  bool logUbx = true;       // log from boot; the LOG touch button toggles
  uint16_t tcpPort = 10110; // SW Maps NMEA server port

#if FEATURE_BLE
  // --- BLE NUS (coex stack only) ---
  bool bleEnable = true;
  char bleName[25] = "PyntRTK-rover";
#endif

#if ENABLE_WEB_CONFIG
  // --- Web config GUI (coex stack only; docs/web-config-spike.md) ---
  bool webEnable = true;      // STA server always-on while surveying
  uint16_t webPort = 80;
  char hostname[33] = "pynt-rtk";   // DHCP hostname (WiFi.setHostname)
  char adminPass[17] = {0};   // empty = generate at first web boot (TRNG),
  char apPass[17] = {0};      // shown on the TFT WEB page; web System card
                              // can change them. Never hardcoded defaults.
  bool bootAp = false;        // one-shot: enter AP provisioning at next boot.
                              // Set+saved by the TFT AP button, cleared at
                              // boot before entering AP. Both AP transitions
                              // are reboots: the NINA's socket table doesn't
                              // survive WiFi.end() honestly (bench 2026-07-31
                              // — leaked socks starve getSocket, bind fails,
                              // phantom-accept storm), so AP mode only ever
                              // starts on a freshly reset module.
  // --- GNSS knobs surfaced by the W3 card (keys parsed + persisted now;
  // gnss_config consumption lands with W3 — flagged in the plan) ---
  uint8_t measRateHz = 1;     // CFG-RATE-MEAS (1..5 sane on F9P all-const)
  uint8_t dynModel = 0;       // CFG-NAVSPG-DYNMODEL (0=portable,2,3,4..)
  bool nmeaGsv = true;        // GSV on UART1 (the bandwidth knob)
#endif
};

extern Settings g_settings;

void settingsLoad();   // SD /config.txt -> g_settings (secrets.h seeds bench defaults)
bool settingsSave();   // g_settings -> SD /config.txt (false if no card)
void settingsPrint(Stream& out);  // passwords masked
bool settingsApplyKeyValue(const char* key, const char* value);
