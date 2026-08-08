#include "compass.h"
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <QMC5883LCompass.h>
#include "config.h"

static QMC5883LCompass g_compass;
static Preferences g_prefs;

static float g_offX = 0, g_offY = 0;
static float g_scaleX = 1, g_scaleY = 1;
static bool g_calibrated = false;

static uint32_t g_lastReadMs = 0;
static float g_headingSmoothed = -1;   // -1 = no reading yet
static int16_t g_rawX = 0, g_rawY = 0;
static uint8_t g_i2cAddr = 0;   // 0 = nothing found, else the address that ACKed

// Cheap bus presence check: tries the QMC5883L address first, then the
// HMC5883L-clone address, so a live trace can tell "no chip on the bus"
// apart from "it's the wrong chip" without a fresh boot/serial capture.
static uint8_t checkI2cAddress() {
    Wire.beginTransmission(0x0D);
    if (Wire.endTransmission() == 0) return 0x0D;
    Wire.beginTransmission(0x1E);
    if (Wire.endTransmission() == 0) return 0x1E;
    return 0;
}

static bool g_calActive = false;
static int g_calMinX, g_calMaxX, g_calMinY, g_calMaxY;
static bool g_calBinVisited[36];

static void applyCalibration() {
    g_compass.setCalibrationOffsets(g_offX, g_offY, 0);
    g_compass.setCalibrationScales(g_scaleX, g_scaleY, 1);
}

static void loadFromPrefs() {
    g_prefs.begin("compass", true);
    g_calibrated = g_prefs.getBool("cal", false);
    g_offX = g_prefs.getFloat("offX", 0);
    g_offY = g_prefs.getFloat("offY", 0);
    g_scaleX = g_prefs.getFloat("sclX", 1);
    g_scaleY = g_prefs.getFloat("sclY", 1);
    g_prefs.end();
}

static void saveToPrefs() {
    g_prefs.begin("compass", false);
    g_prefs.putBool("cal", true);
    g_prefs.putFloat("offX", g_offX);
    g_prefs.putFloat("offY", g_offY);
    g_prefs.putFloat("sclX", g_scaleX);
    g_prefs.putFloat("sclY", g_scaleY);
    g_prefs.end();
}

// QMC5883L lives at I2C address 0x0D and its Chip ID register (also 0x0D)
// should read back 0xFF. Some GY-271 boards actually carry an HMC5883L
// (address 0x1E) instead — detect that case and warn loudly rather than
// silently reporting garbage headings.
static void probeChipId() {
    Wire.beginTransmission(0x0D);
    Wire.write(0x0D);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom((int)0x0D, 1) == 1) {
        uint8_t id = Wire.read();
        Serial.printf("Compass: QMC5883L found, chip ID 0x%02X (expect 0xFF)\n", id);
        return;
    }
    Wire.beginTransmission(0x1E);
    if (Wire.endTransmission() == 0) {
        Serial.println("Compass: WARNING - found a device at 0x1E (HMC5883L "
                        "address). This GY-271 may be an HMC5883L clone, not "
                        "QMC5883L - headings will be wrong with this firmware.");
        return;
    }
    Serial.println("Compass: WARNING - no device found on I2C bus, check "
                    "wiring (SDA/SCL/VCC/GND).");
}

void compassInit() {
    Wire.begin(COMPASS_SDA_PIN, COMPASS_SCL_PIN);
    probeChipId();
    g_compass.init();
    loadFromPrefs();
    applyCalibration();
    Serial.println(g_calibrated ? "Compass: loaded saved calibration"
                                 : "Compass: not calibrated yet");
}

static int headingBin(float heading) { return ((int)heading / 10) % 36; }

void compassUpdate() {
    uint32_t now = millis();
    if (now - g_lastReadMs < COMPASS_READ_MS) return;
    g_lastReadMs = now;

    g_i2cAddr = checkI2cAddress();

    g_compass.read();
    int x = g_compass.getX(), y = g_compass.getY();
    g_rawX = (int16_t)x;
    g_rawY = (int16_t)y;

    if (g_calActive) {
        g_calMinX = min(g_calMinX, x); g_calMaxX = max(g_calMaxX, x);
        g_calMinY = min(g_calMinY, y); g_calMaxY = max(g_calMaxY, y);
        float rawHeading = atan2f((float)y, (float)x) * 180.0f / PI;
        if (rawHeading < 0) rawHeading += 360;
        g_calBinVisited[headingBin(rawHeading)] = true;
    }

    if (!g_calibrated) return;   // no meaningful heading before calibration

    float heading = atan2f((float)y, (float)x) * 180.0f / PI;
    if (heading < 0) heading += 360;

    if (g_headingSmoothed < 0) {
        g_headingSmoothed = heading;
    } else {
        float diff = heading - g_headingSmoothed;
        if (diff > 180) diff -= 360;
        if (diff < -180) diff += 360;
        g_headingSmoothed += diff * 0.3f;
        if (g_headingSmoothed < 0) g_headingSmoothed += 360;
        if (g_headingSmoothed >= 360) g_headingSmoothed -= 360;
    }
}

bool compassHeadingValid() { return g_calibrated && g_headingSmoothed >= 0; }

int compassHeadingDeg() {
    return compassHeadingValid() ? (int)g_headingSmoothed : -1;
}

void compassRawXY(int16_t& x, int16_t& y) {
    x = g_rawX;
    y = g_rawY;
}

uint8_t compassI2cAddr() { return g_i2cAddr; }

void compassCalStart() {
    g_calActive = true;
    g_calMinX = g_calMinY = 32767;
    g_calMaxX = g_calMaxY = -32768;
    memset(g_calBinVisited, 0, sizeof(g_calBinVisited));
    // Sample truly raw values, independent of any previously-saved calibration.
    g_compass.setCalibrationOffsets(0, 0, 0);
    g_compass.setCalibrationScales(1, 1, 1);
    Serial.println("Compass: calibration started");
}

bool compassCalStop(bool save) {
    g_calActive = false;
    bool saved = false;
    if (save && g_calMaxX > g_calMinX && g_calMaxY > g_calMinY) {
        g_offX = (g_calMaxX + g_calMinX) / 2.0f;
        g_offY = (g_calMaxY + g_calMinY) / 2.0f;
        float rangeX = (g_calMaxX - g_calMinX) / 2.0f;
        float rangeY = (g_calMaxY - g_calMinY) / 2.0f;
        float avgRange = (rangeX + rangeY) / 2.0f;
        g_scaleX = avgRange / rangeX;
        g_scaleY = avgRange / rangeY;
        g_calibrated = true;
        saveToPrefs();
        saved = true;
        Serial.println("Compass: calibration saved");
    } else {
        Serial.println(save ? "Compass: calibration data insufficient, not saved"
                             : "Compass: calibration cancelled");
    }
    applyCalibration();   // restore the (old or newly computed) real calibration
    return saved;
}

bool compassCalActive() { return g_calActive; }

uint8_t compassCalCoveragePct() {
    if (!g_calActive) return 0;
    int visited = 0;
    for (int i = 0; i < 36; i++) if (g_calBinVisited[i]) visited++;
    return (uint8_t)(visited * 100 / 36);
}
