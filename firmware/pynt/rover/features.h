// Compile-time feature gates, same pattern as RTK-Feather's features.h.
// Flip only with the matching dependency/wiring in place — stubs #error
// on misuse rather than half-building.
#pragma once

#define FEATURE_NTRIP 1     // WiFi + caster corrections (AirLift)
#define FEATURE_TCP_NMEA 1  // NMEA server on :10110 (QField et al.)
#define FEATURE_SD_LOG 1    // RAWX/SFRBX .ubx logging to the Pynt's microSD
#define FEATURE_BASE 1      // survey-in/fixed + RTCM3-on-UART2 (config only — socket stays empty)

#ifndef FEATURE_BLE
#define FEATURE_BLE 0  // BLE NUS phone link (iOS SW Maps, Q19). Off in the
                       // shipping pynt-rover env — pynt-rover-coex sets
                       // -DFEATURE_BLE=1. Needs nina-fw >= 3.0.0 on the
                       // AirLift + the upstream host stack.
#endif
#if FEATURE_BLE && !defined(COEX_UPSTREAM_NINA)
#error "FEATURE_BLE needs the upstream WiFiNINA/SpiNINA stack (COEX_UPSTREAM_NINA env)"
#endif
#if FEATURE_BLE && !FEATURE_TCP_NMEA
#error "FEATURE_BLE rides tcp_nmea's NMEA drain — enable FEATURE_TCP_NMEA too"
#endif
