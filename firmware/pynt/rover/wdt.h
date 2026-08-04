// SAMD51 watchdog + freeze breadcrumb (team review wedge cluster; bench
// 2026-08-02: eight superloop freezes, every one needed a finger on
// RESET). Armed at the END of setup() — boot's legitimately slow work
// (SD free-space scan ~2:20 on the 32 GB card) happens unarmed. Period
// 16 s: the longest legitimate mid-run stalls measured on this bench are
// the ~10.9 s WiFi.begin() join window and the ~8.4 s log-start file
// allocation; anything past 16 s IS a hang for this device.
#pragma once
#include <stdint.h>

// Superloop module IDs for the breadcrumb (which module a frozen pass
// died in). Keep in sync with kWdtModuleNames in wdt.cpp.
enum : uint8_t {
  WDT_MOD_IDLE = 0,
  WDT_MOD_GNSS,
  WDT_MOD_NTRIP,
  WDT_MOD_TCP,
  WDT_MOD_BLE,
  WDT_MOD_SD,
  WDT_MOD_UI,
  WDT_MOD_MENU,
  WDT_MOD_WEB,
};

void wdtInit();                  // report prior WDT reset (if any) + arm
void wdtKick();                  // once per loop pass
void wdtNoteModule(uint8_t m);   // before entering each module's poll
const char* wdtLastFreeze();     // "" unless the last reset was the WDT;
                                 // else the module name it froze in
