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
void webConfigRequestApMode();   // AP button: enter AP provisioning, or
                                 // (if active) exit via reboot — the only
                                 // reliable AP->STA path (WiFi.end() wedge,
                                 // docs/web-config-spike.md)
bool webConfigApRequested();     // = AP mode active (legacy name)
bool webConfigApActive();        // ntripPoll stands down while true
const char* webConfigAdminPass();  // generated at first boot if unset —
const char* webConfigApPass();     // displayed on the TFT WEB page
const char* webConfigApSsid();     // "<hostname>-setup"
