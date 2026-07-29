#pragma once
#include <Arduino.h>

// Web config GUI — ENABLE_WEB_CONFIG only (pynt-rover-coex env). Serves
// the single-page UI (gzipped PROGMEM) + JSON API on g_settings.webPort.
// No no-op stubs: call sites are #if ENABLE_WEB_CONFIG gated so the
// shipping build stays byte-identical.
void webConfigInit();  // after ntripInit (SPI link up) + settingsLoad
void webConfigPoll();  // lowest-priority residual: one bounded read OR
                       // write per loop pass, single connection at a time

// Touch-UI hooks (WEB page):
void webConfigRequestApMode();   // on-demand AP provisioning entry — W2;
                                 // stub logs until the W0 spike answers
                                 // beginAP()+BLE coexistence
bool webConfigApRequested();
const char* webConfigAdminPass();  // generated at first boot if unset —
const char* webConfigApPass();     // displayed on the TFT WEB page
