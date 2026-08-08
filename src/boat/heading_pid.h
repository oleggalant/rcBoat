#pragma once

// Heading-hold: whenever throttle is applied and the driver's yaw stick is
// centered, locks onto the current compass heading and corrects for drift
// (prop torque, wind, current). The yaw stick fully overrides correction
// while turning; the instant it returns to center, a new heading is captured
// and held — the same behavior as an RC "heading-hold gyro".
//
// Returns the yaw value to feed into motorsSet() in place of driverYaw.
// Falls back to driverYaw unchanged whenever disengaged, uncalibrated, or
// heading-hold is disabled.
float headingHoldUpdate(float throttle, float driverYaw, bool enabled);
