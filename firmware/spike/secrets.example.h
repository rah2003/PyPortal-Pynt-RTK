// Copy to secrets.h (gitignored, same **/secrets.h rule as the rover's)
// for the spare-board coexistence spikes. One network is enough — the
// spikes are bench tests, not field firmware.
#pragma once

#define SPIKE_WIFI_SSID "your-iphone-hotspot"
#define SPIKE_WIFI_PASS "hotspot-password"
