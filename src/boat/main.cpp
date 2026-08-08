// Boat firmware: ESP-NOW control in -> compass heading-hold -> differential
// ESC PWM out. Failsafe semantics match the original WiFi rcBoat: 3 s ESC arm
// at boot, motors cut to idle if no control packet arrives within WATCHDOG_MS.

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "common/espnow_link.h"
#include "protocol.h"
#include "motors.h"
#include "compass.h"
#include "heading_pid.h"

static Preferences g_settingsPrefs;
static uint16_t g_minRunUs = DEFAULT_MIN_RUN_US;
static bool g_headingHoldEnabled = DEFAULT_HEADING_HOLD_ON;
static uint32_t g_calJustSavedUntil = 0;

static void loadSettings() {
    g_settingsPrefs.begin("settings", true);
    g_minRunUs = g_settingsPrefs.getUShort("minRunUs", DEFAULT_MIN_RUN_US);
    g_headingHoldEnabled = g_settingsPrefs.getBool("hHold", DEFAULT_HEADING_HOLD_ON);
    g_settingsPrefs.end();
}

static void saveSettings() {
    g_settingsPrefs.begin("settings", false);
    g_settingsPrefs.putUShort("minRunUs", g_minRunUs);
    g_settingsPrefs.putBool("hHold", g_headingHoldEnabled);
    g_settingsPrefs.end();
}

// Remap [0,1] throttle onto a floor so any nonzero stick jumps straight to
// g_minRunUs instead of crawling up from ESC_MIN_US, where the prop may not
// even be spinning yet. motors.cpp's mixing math is left untouched.
static float applyThrottleFloor(float throttle) {
    if (throttle <= 0.0f) return 0.0f;
    float floorFrac = (g_minRunUs - ESC_MIN_US) / (float)(ESC_MAX_US - ESC_MIN_US);
    floorFrac = constrain(floorFrac, 0.0f, 1.0f);
    return floorFrac + throttle * (1.0f - floorFrac);
}

void setup() {
    Serial.begin(115200);
    motorsInit();               // blocks ESC_ARM_DELAY_MS for arming
    loadSettings();
    compassInit();
    espnowLinkInit(true);
}

void loop() {
    espnowLinkService();
    const uint32_t now = millis();

    if (espnowLinkPaired()) {
        int16_t x, y;
        if (espnowLinkGetControl(x, y)) {
            float throttleF = y / 1000.0f;
            float yaw = headingHoldUpdate(throttleF, x / 1000.0f, g_headingHoldEnabled);
            motorsSet(applyThrottleFloor(throttleF), yaw);

            static uint32_t lastLog = 0;
            if (now - lastLog >= 1000) {
                lastLog = now;
                Serial.printf("Control: x=%d y=%d rssi=%d heading=%d\n",
                              x, y, espnowLinkLastRxRssi(), compassHeadingDeg());
            }
        }
    }

    compassUpdate();

    // Settings from the phone (throttle floor / heading-hold toggle)
    uint16_t newMinRunUs; bool newHeadingHold;
    if (espnowLinkGetSettings(newMinRunUs, newHeadingHold)) {
        g_minRunUs = newMinRunUs;
        g_headingHoldEnabled = newHeadingHold;
        saveSettings();
        Serial.printf("Settings: minRunUs=%u headingHold=%d\n", g_minRunUs, g_headingHoldEnabled);
    }

    // Compass calibration commands from the phone
    uint8_t calCmd;
    if (espnowLinkGetCalCommand(calCmd)) {
        switch (calCmd) {
            case CAL_START:
                compassCalStart();
                break;
            case CAL_SAVE:
                if (compassCalStop(true)) g_calJustSavedUntil = now + 3000;
                break;
            case CAL_CANCEL:
                compassCalStop(false);
                break;
        }
    }

    // Watchdog: link silent -> cut motors (identical to the WiFi version)
    static bool wdTripped = false;
    const uint32_t lastControl = espnowLinkLastControlMs();
    if (lastControl > 0 && now - lastControl > WATCHDOG_MS) {
        motorsStop();
        if (!wdTripped) {
            wdTripped = true;
            Serial.println("Watchdog: link lost, motors stopped");
        }
    } else {
        wdTripped = false;
    }

    static uint32_t lastTelemetry = 0;
    uint32_t telemetryInterval = compassCalActive() ? TELEMETRY_CAL_MS : TELEMETRY_MS;
    if (espnowLinkPaired() && now - lastTelemetry >= telemetryInterval) {
        lastTelemetry = now;
        uint8_t calState = CAL_STATE_IDLE;
        if (compassCalActive()) calState = CAL_STATE_CALIBRATING;
        else if (now < g_calJustSavedUntil) calState = CAL_STATE_SAVED;
        int16_t rawX, rawY;
        compassRawXY(rawX, rawY);
        espnowLinkSendTelemetry(espnowLinkLastRxRssi(), espnowLinkTakeLossPct(),
                                 (int16_t)compassHeadingDeg(), calState,
                                 compassCalCoveragePct(), rawX, rawY, compassI2cAddr());
    }
}
