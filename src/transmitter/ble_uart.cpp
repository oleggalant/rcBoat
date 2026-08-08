#include "ble_uart.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "config.h"

// Nordic UART Service — standard UUIDs so nRF Connect works for debugging.
// Settings/cal characteristics are our own extension, numbered to follow on.
static const char* NUS_SVC      = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* NUS_RX       = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // phone -> TX
static const char* NUS_TX       = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // TX -> phone
static const char* SETTINGS_RX  = "6E400004-B5A3-F393-E0A9-E50E24DCCA9E";  // phone -> TX
static const char* CAL_CMD_RX   = "6E400005-B5A3-F393-E0A9-E50E24DCCA9E";  // phone -> TX

static NimBLECharacteristic* g_txChar = nullptr;
static volatile bool g_connected = false;
static volatile int16_t g_x = 0, g_y = 0;
static volatile bool g_dirty = false;
static volatile uint32_t g_lastWriteMs = 0;

static volatile uint16_t g_setMinRunUs = 0;
static volatile uint8_t g_setHeadingHold = 0;
static volatile bool g_settingsDirty = false;
static volatile uint8_t g_calCmd = 0;
static volatile bool g_calCmdDirty = false;

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        NimBLEAttValue v = c->getValue();
        if (v.length() != 4) return;
        const uint8_t* d = v.data();
        g_x = (int16_t)(d[0] | (d[1] << 8));
        g_y = (int16_t)(d[2] | (d[3] << 8));
        g_lastWriteMs = millis();
        g_dirty = true;
    }
};

class SettingsCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        NimBLEAttValue v = c->getValue();
        if (v.length() != 3) return;
        const uint8_t* d = v.data();
        g_setMinRunUs = (uint16_t)(d[0] | (d[1] << 8));
        g_setHeadingHold = d[2];
        g_settingsDirty = true;
    }
};

class CalCmdCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        NimBLEAttValue v = c->getValue();
        if (v.length() != 1) return;
        g_calCmd = v.data()[0];
        g_calCmdDirty = true;
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, ble_gap_conn_desc* desc) override {
        g_connected = true;
        // 30–50 ms connection interval, 4 s supervision timeout: fast enough
        // for control, relaxed enough to coexist with ESP-NOW on one radio.
        s->updateConnParams(desc->conn_handle, 24, 40, 0, 400);
        Serial.println("BLE: phone connected");
    }
    void onDisconnect(NimBLEServer* s) override {
        g_connected = false;
        g_x = 0;                // failsafe: neutral stick
        g_y = 0;
        g_dirty = true;
        Serial.println("BLE: phone disconnected");
        NimBLEDevice::startAdvertising();
    }
};

void bleUartInit() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEServer* srv = NimBLEDevice::createServer();
    srv->setCallbacks(new ServerCallbacks());

    NimBLEService* svc = srv->createService(NUS_SVC);
    NimBLECharacteristic* rx = svc->createCharacteristic(
        NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rx->setCallbacks(new RxCallbacks());
    g_txChar = svc->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic* settingsChar = svc->createCharacteristic(
        SETTINGS_RX, NIMBLE_PROPERTY::WRITE);
    settingsChar->setCallbacks(new SettingsCallbacks());

    NimBLECharacteristic* calChar = svc->createCharacteristic(
        CAL_CMD_RX, NIMBLE_PROPERTY::WRITE);
    calChar->setCallbacks(new CalCmdCallbacks());

    svc->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SVC);
    adv->setScanResponse(true);     // device name goes in the scan response
    NimBLEDevice::startAdvertising();
    Serial.printf("BLE: advertising as %s\n", BLE_DEVICE_NAME);
}

bool bleUartConnected() { return g_connected; }

bool bleUartGetControl(int16_t& x, int16_t& y) {
    x = g_x;
    y = g_y;
    bool fresh = g_dirty;
    g_dirty = false;
    return fresh;
}

uint32_t bleUartLastWriteMs() { return g_lastWriteMs; }

bool bleUartGetSettings(uint16_t& minRunUs, bool& headingHoldEnabled) {
    minRunUs = g_setMinRunUs;
    headingHoldEnabled = g_setHeadingHold != 0;
    bool fresh = g_settingsDirty;
    g_settingsDirty = false;
    return fresh;
}

bool bleUartGetCalCommand(uint8_t& cmd) {
    cmd = g_calCmd;
    bool fresh = g_calCmdDirty;
    g_calCmdDirty = false;
    return fresh;
}

void bleUartNotifyTelemetry(int8_t rssi, int16_t headingDeg, uint8_t calState,
                             uint8_t calCoveragePct, int16_t rawX, int16_t rawY,
                             uint8_t i2cAddr) {
    if (!g_connected || !g_txChar) return;
    char buf[112];
    int n = snprintf(buf, sizeof(buf),
                      "{\"rssi\":%d,\"heading\":%d,\"cal\":%u,\"calPct\":%u,\"rawX\":%d,\"rawY\":%d,\"i2c\":%u}",
                      (int)rssi, (int)headingDeg, (unsigned)calState, (unsigned)calCoveragePct,
                      (int)rawX, (int)rawY, (unsigned)i2cAddr);
    g_txChar->setValue((uint8_t*)buf, n);
    g_txChar->notify();
}
