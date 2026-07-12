// Compile-time feature gates, same pattern as RTK-Feather's features.h.
// Flip only with the matching dependency/wiring in place — stubs #error
// on misuse rather than half-building.
#pragma once

#define FEATURE_NTRIP 1     // WiFi + caster corrections (AirLift)
#define FEATURE_TCP_NMEA 1  // SW Maps NMEA server on :10110
#define FEATURE_SD_LOG 1    // RAWX/SFRBX .ubx logging to the Pynt's microSD
#define FEATURE_BASE 1      // survey-in/fixed + RTCM3-on-UART2 (config only — socket stays empty)
