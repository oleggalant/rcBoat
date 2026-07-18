#pragma once

// Initialize ESC PWM outputs and run the arming sequence.
// Blocks for ESC_ARM_DELAY_MS ms — call once in setup() before any other code
// that might interfere with the PWM signal.
void motorsInit();

// Set motor speeds from joystick axes.
//   throttle : forward thrust [-1, 1]  (negative values clamped to 0 — no reverse)
//   yaw      : turn rate      [-1, 1]  (positive = turn right)
// Differential mixing: left = throttle + yaw,  right = throttle - yaw
void motorsSet(float throttle, float yaw);

// Cut both motors to idle (ESC_MIN_US).  Called on failsafe / disconnect.
void motorsStop();
