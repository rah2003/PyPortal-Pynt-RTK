#include "wdt.h"

#include <Arduino.h>

namespace {

// Survives a WDT reset: the SAMD51's 8 KB backup RAM at 0x47000000 —
// outside the linker's world entirely, so neither .data init nor .bss
// zeroing can touch it. (First attempt used a .noinit section; the
// linker folded it into .data and startup re-initialized the crumb on
// every boot — verified via nm, symbol type 'd'.) Magic guards against
// garbage on a cold power-on.
constexpr uint32_t kBreadcrumbMagic = 0x57444742;  // "WDGB"
#define crumbMagic (*(volatile uint32_t*)0x47000000)
#define crumbModule (*(volatile uint32_t*)0x47000004)
// Separate crumb set for hard faults: the fault handler resets via
// NVIC_SystemReset, so RCAUSE reads SYST (0x40), not WDT — the boot
// check for these must not gate on reset cause.
constexpr uint32_t kFaultMagic = 0x48464C54;  // "HFLT"
#define faultMagic (*(volatile uint32_t*)0x47000010)
#define faultPc (*(volatile uint32_t*)0x47000014)
#define faultLr (*(volatile uint32_t*)0x47000018)
#define faultCfsr (*(volatile uint32_t*)0x4700001C)
#define faultHfsr (*(volatile uint32_t*)0x47000020)

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

// A HardFault outranks every configurable IRQ, so the core's default
// while(1) fault handler starves even the priority-0 EW interrupt and
// the device sits dead until the 16 s hardware reset — no breadcrumb,
// nothing. Override it: pull the stacked PC/LR off whichever stack was
// active (EXC_RETURN bit 2 picks MSP vs PSP), stamp them to backup RAM,
// reset immediately. Naked so the compiler prologue can't disturb LR
// before we test it.
extern "C" void HardFault_Stamp(uint32_t* sp) {
  faultMagic = kFaultMagic;
  faultPc = sp[6];
  faultLr = sp[5];
  faultCfsr = SCB->CFSR;  // UFSR[31:16] BFSR[15:8] MMFSR[7:0]
  faultHfsr = SCB->HFSR;  // FORCED bit30 = escalated configurable fault
  crumbMagic = kBreadcrumbMagic;
  crumbModule = currentModule;
  NVIC_SystemReset();
}

extern "C" __attribute__((naked)) void HardFault_Handler(void) {
  __asm volatile(
      "tst lr, #4        \n"
      "ite eq            \n"
      "mrseq r0, msp     \n"
      "mrsne r0, psp     \n"
      "b HardFault_Stamp \n");
}

void wdtInit() {
  MCLK->AHBMASK.bit.BKUPRAM_ = 1;  // backup RAM clock (default-on; be sure)
  // Self-test: if BKUPRAM writes fault or don't stick, the EW handler
  // would hard-fault mid-freeze and the breadcrumb story is dead — find
  // out HERE, in thread mode, with a printout.
  {
    volatile uint32_t* probe = (volatile uint32_t*)0x47000008;
    *probe = 0x12345678;
    Serial.print(F("[wdt] bkupram self-test: "));
    Serial.println(*probe == 0x12345678 ? F("ok") : F("FAILED"));
  }
  // .reg + mask, not .bit.WDT — the bitfield name collides with the WDT
  // peripheral macro and macro-expands into gibberish.
  Serial.print(F("[wdt] reset cause 0x"));
  Serial.println(RSTC->RCAUSE.reg, HEX);
  if ((RSTC->RCAUSE.reg & RSTC_RCAUSE_WDT) && crumbMagic == kBreadcrumbMagic &&
      crumbModule < sizeof(kWdtModuleNames) / sizeof(kWdtModuleNames[0])) {
    strncpy(lastFreeze, kWdtModuleNames[crumbModule], sizeof(lastFreeze) - 1);
    Serial.print(F("[wdt] WATCHDOG RESET — loop froze in: "));
    Serial.println(lastFreeze);
  }
  // Not gated on RCAUSE: the fault handler exits via SystemReset (0x40).
  if (faultMagic == kFaultMagic) {
    Serial.print(F("[wdt] HARDFAULT pc=0x"));
    Serial.print(faultPc, HEX);
    Serial.print(F(" lr=0x"));
    Serial.print(faultLr, HEX);
    Serial.print(F(" cfsr=0x"));
    Serial.print(faultCfsr, HEX);
    Serial.print(F(" hfsr=0x"));
    Serial.print(faultHfsr, HEX);
    Serial.print(F(" in: "));
    Serial.println(crumbMagic == kBreadcrumbMagic &&
                           crumbModule < sizeof(kWdtModuleNames) /
                                             sizeof(kWdtModuleNames[0])
                       ? kWdtModuleNames[crumbModule]
                       : "?");
  }
  faultMagic = 0;
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

// Marking a module boundary also kicks: the watchdog then measures ONE
// MODULE's stall, not the whole pass. Bench 2026-08-03: the first loop
// pass after boot legitimately runs >16 s (32 KB SERCOM backlog parse +
// ~11 s WiFi join + caster connect + server binds) and a single
// end-of-pass kick boot-looped the device on the first WDT period.
void wdtNoteModule(uint8_t m) {
  currentModule = m;
  wdtKick();
}

const char* wdtLastFreeze() { return lastFreeze; }
