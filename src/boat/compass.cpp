#include "compass.h"
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_QMC5883P.h>
#include "config.h"

static Adafruit_QMC5883P g_compass;
static Preferences g_prefs;

// Hard-iron calibration (X/Y only — the boat stays level, so Z/tilt
// compensation is skipped). Applied here in software on every reading,
// since Adafruit_QMC5883P has no built-in offset/scale hooks.
static float g_offX = 0, g_offY = 0;
static float g_scaleX = 1, g_scaleY = 1;
static bool g_calibrated = false;

static uint32_t g_lastReadMs = 0;
static float g_headingSmoothed = -1;   // -1 = no reading yet
static int16_t g_rawX = 0, g_rawY = 0;
static uint8_t g_i2cAddr = 0;   // 0 = nothing found, else the address that ACKed

static bool g_calActive = false;
static int g_calMinX, g_calMaxX, g_calMinY, g_calMaxY;
static bool g_calBinVisited[36];

// Many boards sold as "GY-271 QMC5883L" now actually ship the newer
// QMC5883P chip instead, at a different I2C address (0x2C vs 0x0D) — check
// that one first, then the two older-chip addresses so a live trace can
// still tell "wrong/old chip" apart from "nothing on the bus".
static uint8_t checkI2cAddress() {
    Wire.beginTransmission(0x2C);
    if (Wire.endTransmission() == 0) return 0x2C;
    Wire.beginTransmission(0x0D);
    if (Wire.endTransmission() == 0) return 0x0D;
    Wire.beginTransmission(0x1E);
    if (Wire.endTransmission() == 0) return 0x1E;
    return 0;
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

void compassInit() {
    Wire.begin(COMPASS_SDA_PIN, COMPASS_SCL_PIN);
    g_i2cAddr = checkI2cAddress();

    if (g_compass.begin(0x2C, &Wire)) {
        g_compass.setMode(QMC5883P_MODE_CONTINUOUS);
        g_compass.setODR(QMC5883P_ODR_100HZ);
        g_compass.setRange(QMC5883P_RANGE_8G);
        g_compass.setOSR(QMC5883P_OSR_8);
        g_compass.setDSR(QMC5883P_DSR_1);
        g_compass.setSetResetMode(QMC5883P_SETRESET_ON);
        Serial.println("Compass: QMC5883P initialized");
    } else {
        Serial.printf("Compass: WARNING - QMC5883P begin() failed (I2C probe found "
                       "address 0x%02X, expected 0x2C) - check wiring/chip type\n", g_i2cAddr);
    }

    loadFromPrefs();
    Serial.println(g_calibrated ? "Compass: loaded saved calibration"
                                 : "Compass: not calibrated yet");
}

static int headingBin(float heading) { return ((int)heading / 10) % 36; }

void compassUpdate() {
    uint32_t now = millis();
    if (now - g_lastReadMs < COMPASS_READ_MS) return;
    g_lastReadMs = now;

    g_i2cAddr = checkI2cAddress();

    int16_t rawX, rawY, rawZ;
    if (g_compass.getRawMagnetic(&rawX, &rawY, &rawZ)) {
        g_rawX = rawX;
        g_rawY = rawY;
    }
    // else: leave g_rawX/g_rawY at their last value — a stuck trace reading
    // is exactly the signal a wiring/chip problem should produce.

    if (g_calActive) {
        g_calMinX = min(g_calMinX, (int)g_rawX); g_calMaxX = max(g_calMaxX, (int)g_rawX);
        g_calMinY = min(g_calMinY, (int)g_rawY); g_calMaxY = max(g_calMaxY, (int)g_rawY);
        float rawHeading = atan2f((float)g_rawY, (float)g_rawX) * 180.0f / PI;
        if (rawHeading < 0) rawHeading += 360;
        g_calBinVisited[headingBin(rawHeading)] = true;
    }

    if (!g_calibrated) return;   // no meaningful heading before calibration

    float x = (g_rawX - g_offX) * g_scaleX;
    float y = (g_rawY - g_offY) * g_scaleY;
    float heading = atan2f(y, x) * 180.0f / PI;
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
    return saved;
}

bool compassCalActive() { return g_calActive; }

uint8_t compassCalCoveragePct() {
    if (!g_calActive) return 0;
    int visited = 0;
    for (int i = 0; i < 36; i++) if (g_calBinVisited[i]) visited++;
    return (uint8_t)(visited * 100 / 36);
}
