// Copy to secrets.h (gitignored) for bench convenience. These seed the
// defaults at boot; SD /config.txt overrides them. Never commit real
// credentials — same policy as the Feather and Metro builds.
#pragma once

#define SECRET_WIFI_SSID1 "your-iphone-hotspot"
#define SECRET_WIFI_PASS1 "hotspot-password"

// NTRIP caster credentials (host/port/mount default in settings.h):
#define SECRET_CASTER_USER "username"
#define SECRET_CASTER_PASS "password"
