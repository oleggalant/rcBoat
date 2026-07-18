// Boat firmware: ESP-NOW control in -> differential ESC PWM out.
// Failsafe semantics match the original WiFi rcBoat: 3 s ESC arm at boot,
// motors cut to idle if no control packet arrives within WATCHDOG_MS.

#include <Arduino.h>
#include "config.h"
#include "common/espnow_link.h"
#include "motors.h"

void setup() {
    Serial.begin(115200);
    motorsInit();               // blocks ESC_ARM_DELAY_MS for arming
    espnowLinkInit(true);
}

void loop() {
    espnowLinkService();
    const uint32_t now = millis();

    int16_t x, y;
    if (espnowLinkGetControl(x, y)) {
        motorsSet(y / 1000.0f, x / 1000.0f);   // throttle = y, yaw = x

        static uint32_t lastLog = 0;
        if (now - lastLog >= 1000) {
            lastLog = now;
            Serial.printf("Control: x=%d y=%d rssi=%d\n", x, y, espnowLinkLastRxRssi());
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
    if (espnowLinkPaired() && now - lastTelemetry >= TELEMETRY_MS) {
        lastTelemetry = now;
        espnowLinkSendTelemetry(espnowLinkLastRxRssi(), espnowLinkTakeLossPct());
    }
}
