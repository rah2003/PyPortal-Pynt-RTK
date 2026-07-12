#pragma once
#include <Arduino.h>
#include <SdFat.h>

// RAWX/SFRBX .ubx logger on the Pynt's own microSD. Ported from the M0
// Adalogger rover (RTK-Feather), minus the status link: GNSS time comes
// straight from g_gnss (same MCU), and log start/stop comes from the
// touch UI / serial menu instead of a LOGCTL message.
bool sdLoggerInit();        // sd.begin + one-time free-space scan; before settingsLoad()
void sdLoggerPoll();        // open/rotate/drain/flush; call every loop pass
void sdLoggerSetEnabled(bool on);  // touch button / serial menu
void sdLoggerShutdown();    // close file; stays closed until reset ("safe to power off")

bool sdIsOk();
SdFat& sdCard();            // shared with settings.cpp (/config.txt)
