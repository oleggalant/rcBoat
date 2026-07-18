// Transmitter firmware: BLE joystick in (phone) -> ESP-NOW control out (boat).
//
// Failsafes:
//  - BLE disconnect or no BLE write for BLE_WATCHDOG_MS -> send neutral (0,0).
//  - Control is retransmitted every HEARTBEAT_MS regardless of change, so the
//    boat's 500 ms watchdog stays fed while the stick is held steady.
//  - No packet from the boat for LINK_TIMEOUT_MS -> back to discovery.

#include <Arduino.h>
#include "config.h"
#include "common/espnow_link.h"
#include "ble_uart.h"

void setup() {
    Serial.begin(115200);
    espnowLinkInit(false);      // WiFi/ESP-NOW before NimBLE for clean coex init
    bleUartInit();
}

void loop() {
    espnowLinkService();
    const uint32_t now = millis();

    // Pairing state machine
    if (!espnowLinkPaired()) {
        static uint32_t lastDiscover = 0;
        if (now - lastDiscover >= DISCOVERY_MS) {
            lastDiscover = now;
            espnowLinkSendDiscover();
        }
    } else if (now - espnowLinkLastRxMs() > LINK_TIMEOUT_MS) {
        Serial.println("Link: boat silent, re-discovering");
        espnowLinkResetPairing();
        bleUartNotifyRssi(0);   // phone bars go dark
    }

    // Control values with BLE failsafe
    int16_t x, y;
    bool fresh = bleUartGetControl(x, y);
    const uint32_t lastWrite = bleUartLastWriteMs();
    const bool bleOk = bleUartConnected() && lastWrite > 0 &&
                       now - lastWrite <= BLE_WATCHDOG_MS;
    if (!bleOk) {
        x = 0;
        y = 0;
    }

    // Send immediately on change, otherwise heartbeat the last values
    static uint32_t lastSend = 0;
    if (espnowLinkPaired() && (fresh || now - lastSend >= HEARTBEAT_MS)) {
        lastSend = now;
        espnowLinkSendControl(x, y);
    }

    // Forward boat telemetry to the phone
    int8_t rssi;
    uint8_t lossPct;
    if (espnowLinkGetTelemetry(rssi, lossPct)) {
        bleUartNotifyRssi(rssi);
        Serial.printf("Telemetry: boat rssi %d dBm, loss %u%%\n", rssi, lossPct);
    }
}
