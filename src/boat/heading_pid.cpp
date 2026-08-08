#include "heading_pid.h"
#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "compass.h"

static bool g_holding = false;
static float g_targetHeading = 0;
static float g_integral = 0;
static float g_lastError = 0;
static uint32_t g_lastUpdateMs = 0;

// Wraparound-safe target-minus-current, normalized to [-180, 180].
static float headingError(float target, float current) {
    return fmodf(target - current + 540.0f, 360.0f) - 180.0f;
}

float headingHoldUpdate(float throttle, float driverYaw, bool enabled) {
    if (!enabled || !compassHeadingValid() || throttle <= 0.0f ||
        fabsf(driverYaw) >= HEADING_YAW_DEADBAND) {
        g_holding = false;
        g_integral = 0;
        return driverYaw;
    }

    float current = (float)compassHeadingDeg();
    if (!g_holding) {
        g_targetHeading = current;   // lock onto wherever we're pointed now
        g_integral = 0;
        g_lastError = 0;
        g_holding = true;
    }

    uint32_t now = millis();
    float dt = g_lastUpdateMs ? (now - g_lastUpdateMs) / 1000.0f : 0.05f;
    g_lastUpdateMs = now;
    if (dt <= 0 || dt > 0.5f) dt = 0.05f;   // guard first pass / long gaps

    float error = headingError(g_targetHeading, current);
    g_integral = constrain(g_integral + error * dt, -50.0f, 50.0f);
    float derivative = (error - g_lastError) / dt;
    g_lastError = error;

    float correction = PID_SIGN * (PID_KP * error + PID_KI * g_integral + PID_KD * derivative);
    return constrain(correction, -PID_MAX_CORRECTION, PID_MAX_CORRECTION);
}
