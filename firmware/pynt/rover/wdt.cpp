#include "wdt.h"

#include <Arduino.h>

namespace {

// Survives a WDT reset: .noinit is an orphan NOBITS section the linker
// places between __bss_end__ (where startup zeroing stops) and __end__
// (where the heap starts) — verified against the pyportal_m4 linker
// script. Magic guards against garbage on a cold power-on.
constexpr uint32_t kBreadcrumbMagic = 0x57444742;  // "WDGB"
__attribute__((section(".noinit"))) volatile uint32_t crumbMagic;
__attribute__((section(".noinit"))) volatile uint8_t crumbModule;

volatile uint8_t currentModule = WDT_MOD_IDLE;

const char* const kWdtModuleNames[] = {"idle",  "gnss", "ntrip",
                                       "tcp",   "ble",  "sd",
                                       "ui",    "menu", "web"};

char lastFreeze[8] = "";

}  // namespace

// Early-warning IRQ at 8 s of no kick — the reset itself fires at 16 s.
// Only stamps the breadcrumb; a pass that recovers between 8 and 16 s
// kicks normally and the stamp is simply never read. extern "C": the
// startup vector table references the unmangled symbol.
extern "C" void WDT_Handler(void) {
  WDT->INTFLAG.reg = WDT_INTFLAG_EW;
  crumbMagic = kBreadcrumbMagic;
  crumbModule = currentModule;
}

void wdtInit() {
  // .reg + mask, not .bit.WDT — the bitfield name collides with the WDT
  // peripheral macro and macro-expands into gibberish.
  if ((RSTC->RCAUSE.reg & RSTC_RCAUSE_WDT) && crumbMagic == kBreadcrumbMagic &&
      crumbModule < sizeof(kWdtModuleNames) / sizeof(kWdtModuleNames[0])) {
    strncpy(lastFreeze, kWdtModuleNames[crumbModule], sizeof(lastFreeze) - 1);
    Serial.print(F("[wdt] WATCHDOG RESET — loop froze in: "));
    Serial.println(lastFreeze);
  }
  crumbMagic = 0;

  // SAMD51 WDT runs from its dedicated OSCULP32K-derived 1.024 kHz —
  // no GCLK plumbing. CYC16384 = 16 s period, EW offset CYC8192 = 8 s.
  WDT->CTRLA.bit.ENABLE = 0;
  while (WDT->SYNCBUSY.reg) {}
  WDT->CONFIG.reg = WDT_CONFIG_PER_CYC16384;
  WDT->EWCTRL.reg = WDT_EWCTRL_EWOFFSET_CYC8192;
  WDT->INTENCLR.reg = WDT_INTENCLR_MASK;
  WDT->INTFLAG.reg = WDT_INTFLAG_MASK;
  WDT->INTENSET.reg = WDT_INTENSET_EW;
  NVIC_SetPriority(WDT_IRQn, 0);
  NVIC_EnableIRQ(WDT_IRQn);
  WDT->CTRLA.bit.ENABLE = 1;
  while (WDT->SYNCBUSY.reg) {}
  Serial.println(F("[wdt] armed: 16 s, early warning 8 s"));
}

void wdtKick() {
  // Skip the kick while a previous CLEAR still synchronizes (~100 us at
  // 1.024 kHz) — the loop passes far more often than the sync window.
  if (!WDT->SYNCBUSY.bit.CLEAR)
    WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
}

void wdtNoteModule(uint8_t m) { currentModule = m; }

const char* wdtLastFreeze() { return lastFreeze; }
