// ZED-F9P configuration via UBX-CFG-VALSET. Ported from RTK-Feather's
// gnss_config.cpp (itself from Metro), with one meaningful delta: there
// is no second listener on UART1 anymore — RAWX/SFRBX are still enabled,
// but for THIS MCU's own SparkFun file-buffer logger (sd_logger.cpp), not
// a passive tap. Key IDs verified during the Feather build against the
// pyubx2 configdb mirror and PaulZC's UBX.md (docs/hardware/ucenter-config.md).
//
// Baud stays 115200: the D3/D4 lines carry 1 kΩ series protection
// resistors (docs/hardware/wiring.md) — don't raise past 230400 without
// scoping the edges, and there's no throughput need at 1 Hz.
#include "gnss_config.h"

#include "features.h"
#include "settings.h"

namespace {

constexpr uint8_t kLayers = VAL_LAYER_ALL;

bool applyPorts(SFE_UBLOX_GNSS_SERIAL& g) {
  bool ok = g.newCfgValset(kLayers);
  ok &= g.addCfgValset(UBLOX_CFG_UART1INPROT_UBX, 1);
  ok &= g.addCfgValset(UBLOX_CFG_UART1INPROT_NMEA, 0);
  ok &= g.addCfgValset(UBLOX_CFG_UART1INPROT_RTCM3X, 1);
  ok &= g.addCfgValset(UBLOX_CFG_UART1OUTPROT_UBX, 1);
  ok &= g.addCfgValset(UBLOX_CFG_UART1OUTPROT_NMEA, 1);
  ok &= g.addCfgValset(UBLOX_CFG_UART1OUTPROT_RTCM3X, 0);  // Base uses UART2 (Phase 3)
  // UART2/XBee: deliberately absent — reserved for the future radio.
  ok &= g.sendCfgValset();
  return ok;
}

bool applyMessages(SFE_UBLOX_GNSS_SERIAL& g) {
  bool ok = g.newCfgValset(kLayers);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_RAWX_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_SFRBX_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_UBX_NAV_PVT_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_UART1, 5);  // every 5th epoch
  ok &= g.addCfgValset(UBLOX_CFG_NMEA_HIGHPREC, 1);
  ok &= g.sendCfgValset();
  return ok;
}

bool applyNav(SFE_UBLOX_GNSS_SERIAL& g) {
  bool ok = g.newCfgValset(kLayers);
  ok &= g.addCfgValset(UBLOX_CFG_RATE_MEAS, 1000);  // 1 Hz
  ok &= g.addCfgValset(UBLOX_CFG_RATE_NAV, 1);
  ok &= g.addCfgValset(UBLOX_CFG_NAVSPG_INFIL_MINELEV, g_settings.elevMaskDeg);
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_GPS_ENA, 1);
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_GLO_ENA, 1);
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_GAL_ENA, 1);
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_BDS_ENA, 1);
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_SBAS_ENA, 0);
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_QZSS_ENA, 0);
  ok &= g.sendCfgValset();
  return ok;
}

bool applyRoverTmode(SFE_UBLOX_GNSS_SERIAL& g) {
  // A previous Base session must not leave TMODE latched on. Also turn
  // the base-mode outputs back off (RTCM on UART2, NAV-SVIN on UART1).
  bool ok = g.newCfgValset(kLayers);
  ok &= g.addCfgValset(UBLOX_CFG_TMODE_MODE, 0);
  ok &= g.addCfgValset(UBLOX_CFG_UART2OUTPROT_RTCM3X, 0);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_UBX_NAV_SVIN_UART1, 0);
  ok &= g.sendCfgValset();
  return ok;
}

#if FEATURE_BASE
bool applyBaseTmode(SFE_UBLOX_GNSS_SERIAL& g) {
  // Base mode — HARD CONSTRAINT reminder (README §constraints): this is
  // CONFIG ONLY. RTCM3 goes out on UART2 for the future plug-in radio;
  // the XBee socket stays physically empty and UART2's baud is left at
  // the F9P default (the radio choice sets it later).
  bool ok = g.newCfgValset(kLayers);

  const bool fixed =
      g_settings.fixedLatDeg != 0 || g_settings.fixedLonDeg != 0;
  if (fixed) {
    // Fixed position, LLH. CFG-TMODE-LAT/LON are 1e-7 deg (~1.1 cm
    // granularity); the _HP 1e-9 refinements are deliberately skipped in
    // v1 — survey-in is the precision path, fixed is the convenience path.
    ok &= g.addCfgValset(UBLOX_CFG_TMODE_MODE, 2);      // 2 = FIXED
    ok &= g.addCfgValset(UBLOX_CFG_TMODE_POS_TYPE, 1);  // 1 = LLH
    ok &= g.addCfgValset(UBLOX_CFG_TMODE_LAT,
                         (int32_t)(g_settings.fixedLatDeg * 1e7));
    ok &= g.addCfgValset(UBLOX_CFG_TMODE_LON,
                         (int32_t)(g_settings.fixedLonDeg * 1e7));
    ok &= g.addCfgValset(UBLOX_CFG_TMODE_HEIGHT,
                         (int32_t)(g_settings.fixedAltM * 100));  // cm
  } else {
    ok &= g.addCfgValset(UBLOX_CFG_TMODE_MODE, 1);  // 1 = SURVEY_IN
    ok &= g.addCfgValset(UBLOX_CFG_TMODE_SVIN_MIN_DUR, g_settings.svinMinDurS);
    ok &= g.addCfgValset(UBLOX_CFG_TMODE_SVIN_ACC_LIMIT,
                         g_settings.svinAccLimit01mm);
  }

  // RTCM3 out on UART2 (config only). Standard MSM4 base set + antenna
  // reference point + GLONASS biases (SparkFun RTK / u-blox app-note set).
  ok &= g.addCfgValset(UBLOX_CFG_UART2OUTPROT_RTCM3X, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1005_UART2, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1074_UART2, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1084_UART2, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1094_UART2, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1124_UART2, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1230_UART2, 5);

  // Survey-in progress for the UI (NAV-SVIN on UART1, 1 Hz).
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_UBX_NAV_SVIN_UART1, 1);
  ok &= g.sendCfgValset();
  return ok;
}
#endif

}  // namespace

bool gnssApplyProjectConfig(SFE_UBLOX_GNSS_SERIAL& gnss) {
  bool ok = applyPorts(gnss);
  ok &= applyMessages(gnss);
  ok &= applyNav(gnss);
#if FEATURE_BASE
  if (g_settings.mode == DeviceMode::Base) ok &= applyBaseTmode(gnss);
  else ok &= applyRoverTmode(gnss);
#else
  ok &= applyRoverTmode(gnss);
#endif
  return ok;
}

bool gnssFactoryRecover(SFE_UBLOX_GNSS_SERIAL& gnss) {
  // The baud dance lives in gnss.cpp (it owns SerialGNSS); this performs
  // only the reset + reapply. gnss.cpp re-establishes the link first.
  Serial.println(F("[gnss] factory reset (UBX-CFG-CFG clear/load defaults)..."));
  gnss.factoryDefault();
  delay(5000);  // module reboots
  return true;  // caller redoes the connect + gnssApplyProjectConfig
}
