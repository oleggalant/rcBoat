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

// Last raw X/Y magnetometer counts (calibration-adjusted once calibrated,
// otherwise the true raw reading). Useful as a wiring/bench-test trace: if
// these never change, the I2C bus isn't actually being read.
void compassRawXY(int16_t& x, int16_t& y);

// Live I2C bus presence check (re-probed each compassUpdate()): returns the
// address that ACKed (0x0D = QMC5883L, 0x1E = HMC5883L-clone address), or 0
// if nothing responds. Lets a remote trace tool distinguish "no chip on the
// bus" from "wrong chip" from "chip present but values not moving", without
// needing a fresh boot/serial capture.
uint8_t compassI2cAddr();

void compassCalStart();
bool compassCalStop(bool save);   // true if this call resulted in a fresh save
bool compassCalActive();
uint8_t compassCalCoveragePct();   // 0-100, meaningful only while active
