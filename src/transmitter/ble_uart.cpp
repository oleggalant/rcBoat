#include "ble_uart.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "config.h"

// Nordic UART Service — standard UUIDs so nRF Connect works for debugging
static const char* NUS_SVC = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* NUS_RX  = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // phone -> TX
static const char* NUS_TX  = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // TX -> phone

static NimBLECharacteristic* g_txChar = nullptr;
static volatile bool g_connected = false;
static volatile int16_t g_x = 0, g_y = 0;
static volatile bool g_dirty = false;
static volatile uint32_t g_lastWriteMs = 0;

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

void bleUartNotifyRssi(int8_t rssi) {
    if (!g_connected || !g_txChar) return;
    char buf[24];
    int n = snprintf(buf, sizeof(buf), "{\"rssi\":%d}", (int)rssi);
    g_txChar->setValue((uint8_t*)buf, n);
    g_txChar->notify();
}
