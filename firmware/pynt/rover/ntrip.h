#pragma once
#include <Arduino.h>

// NTRIP client — polled state machine (no RTOS task on this MCU).
// Owns WiFi association and the caster socket; RTCM bytes inject
// directly into the GNSS library (gnssInjectRtcm) — single core, no ring.
void ntripInit();
void ntripPoll();  // call every loop pass; never blocks longer than one
                   // WiFi.begin()/connect() attempt (documented stall,
                   // see the WiFi-join note in ntrip.cpp)
const char* ntripStateName();
#if ENABLE_WEB_CONFIG
void ntripRequestReconnect();  // W3 Corrections card: drop the caster
                               // socket and reconnect promptly so edited
                               // caster settings take effect live
#endif
