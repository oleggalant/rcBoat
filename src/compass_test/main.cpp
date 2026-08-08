// Standalone compass diagnostic firmware — no ESP-NOW/BLE, just I2C.
// Scans the entire bus and fingerprints whichever chip (if any) responds:
// boards sold as "GY-271" have shipped with QMC5883L (0x0D), an
// HMC5883L-clone (0x1E), and increasingly the newer QMC5883P (0x2C) — a
// plain "found/not found" isn't enough to tell them apart. Whichever one is
// confirmed gets its live raw X/Y/Z streamed on the serial monitor, so
// wiring/orientation can be checked directly without the full rcBoat radio
// stack.
//
// Build & flash just this firmware:
//   pio run -e compass_test -t upload
//   pio device monitor

#include <Arduino.h>
#include <Wire.h>
#include <QMC5883LCompass.h>
#include <Adafruit_QMC5883P.h>
#include "config.h"

static QMC5883LCompass g_qmc;
static Adafruit_QMC5883P g_qmcp;
static bool g_qmcpReady = false;
static bool g_qmcReady = false;
static bool g_hmcFound = false;

static void fullScan() {
    Serial.println("--- I2C bus scan (0x03-0x77) ---");
    int found = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  found device at 0x%02X\n", addr);
            found++;
        }
    }
    if (!found) Serial.println("  (nothing responded on the bus)");
}

// HMC5883L identification registers (0x0A/0x0B/0x0C) read back ASCII "H43"
// on genuine chips — a solid fingerprint distinct from QMC5883L.
static bool identifyHmc() {
    Wire.beginTransmission(0x1E);
    Wire.write(0x0A);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)0x1E, 3) != 3) return false;
    char id[4] = {0};
    id[0] = Wire.read(); id[1] = Wire.read(); id[2] = Wire.read();
    Serial.printf("HMC5883L-address chip ID registers: \"%s\" (genuine HMC5883L reads \"H43\")\n", id);
    return true;
}

// QMC5883L chip-ID register (0x0D) should read back 0xFF on genuine chips.
static bool identifyQmc() {
    Wire.beginTransmission(0x0D);
    Wire.write(0x0D);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)0x0D, 1) != 1) return false;
    uint8_t id = Wire.read();
    Serial.printf("QMC5883L chip ID register: 0x%02X (genuine QMC5883L reads 0xFF)\n", id);
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.printf("\nCompass test firmware -- SDA=GPIO%d SCL=GPIO%d\n",
                  COMPASS_SDA_PIN, COMPASS_SCL_PIN);
    Wire.begin(COMPASS_SDA_PIN, COMPASS_SCL_PIN);

    fullScan();

    if (g_qmcp.begin(0x2C, &Wire)) {
        g_qmcp.setMode(QMC5883P_MODE_CONTINUOUS);
        g_qmcp.setODR(QMC5883P_ODR_100HZ);
        g_qmcp.setRange(QMC5883P_RANGE_8G);
        g_qmcpReady = true;
        Serial.println("=> QMC5883P confirmed (chip ID 0x00 == 0x80). Streaming live X/Y/Z below.");
    } else if (identifyQmc()) {
        g_qmc.init();
        g_qmcReady = true;
        Serial.println("=> QMC5883L confirmed. Streaming live X/Y/Z below.");
    } else if (identifyHmc()) {
        g_hmcFound = true;
        Serial.println("=> This is an HMC5883L (or clone). The main rcBoat firmware "
                        "only supports QMC5883P/QMC5883L -- either swap the module or "
                        "a different driver is needed.");
    } else {
        Serial.println("=> No compass chip found at any known address. "
                        "This is a wiring/power problem, not a chip-type problem.");
    }
}

void loop() {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint < 500) return;
    lastPrint = millis();

    if (g_qmcpReady) {
        int16_t x, y, z;
        if (g_qmcp.getRawMagnetic(&x, &y, &z))
            Serial.printf("QMC5883P raw: X=%d Y=%d Z=%d\n", x, y, z);
        else
            Serial.println("QMC5883P: getRawMagnetic() failed (data not ready?)");
    } else if (g_qmcReady) {
        g_qmc.read();
        Serial.printf("QMC5883L raw: X=%d Y=%d Z=%d\n",
                      g_qmc.getX(), g_qmc.getY(), g_qmc.getZ());
    } else if (g_hmcFound) {
        identifyHmc();   // re-print to confirm it's still responding
    } else {
        fullScan();      // keep rescanning so wiring fixes show up live
    }
}
