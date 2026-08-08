#pragma once
#include <stdint.h>

// GY-271 QMC5883L wrapper. Heading-only: hard-iron calibration covers just
// X/Y (the boat stays level on the water, so Z/tilt compensation is skipped).
// Calibration is driven remotely (start/stop over the ESP-NOW link) rather
// than via the library's serial-based sketch, since the boat has no monitor
// cable once it's on the water.

void compassInit();     // Wire.begin + library init + load saved calibration
void compassUpdate();   // call every loop(); internally rate-limited

bool compassHeadingValid();   // false until a calibration has been saved
int  compassHeadingDeg();     // 0-359, or -1 if invalid

void compassCalStart();
bool compassCalStop(bool save);   // true if this call resulted in a fresh save
bool compassCalActive();
uint8_t compassCalCoveragePct();   // 0-100, meaningful only while active
