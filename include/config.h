#pragma once

// ── Boat: ESC outputs (same wiring as the original rcBoat) ──────────────────
#define MOTOR_LEFT_PIN   1
#define MOTOR_RIGHT_PIN  5

// Standard hobby ESC PWM range (microseconds)
#define ESC_MIN_US       1000   // stopped / armed
#define ESC_MAX_US       2000   // full throttle

// Time to hold ESC_MIN_US on boot so ESCs complete arming sequence
#define ESC_ARM_DELAY_MS 3000

// Boat: stop motors if no control packet received for this long (ms)
#define WATCHDOG_MS      500

// ── Radio link ───────────────────────────────────────────────────────────────
#define ESPNOW_CHANNEL   6      // both ends MUST match
#define ESPNOW_USE_LR    1      // 1 = Espressif Long-Range PHY (both ends must
                                // agree!), 0 = standard 802.11bgn for A/B tests
#define HEARTBEAT_MS     50     // TX resends last control at 20 Hz
#define TELEMETRY_MS     2000   // boat -> TX RSSI report period
#define DISCOVERY_MS     500    // TX pairing broadcast period
#define LINK_TIMEOUT_MS  5000   // TX: no packet from boat -> back to discovery

// ── Transmitter: BLE ─────────────────────────────────────────────────────────
#define BLE_DEVICE_NAME  "rcBoatTx"
// Neutralize the boat if the phone stops writing for this long. The web page
// heartbeats every 250 ms, so this only trips on a hung/backgrounded tab.
#define BLE_WATCHDOG_MS  1000

// ── Boat: compass (GY-271 QMC5883L) ─────────────────────────────────────────
// GPIO8 avoided: it's hardwired to the ESP32-C3-DevKitM-1's onboard
// addressable RGB LED, which can interfere with I2C on that pin. GPIO9
// avoided too: it's a boot-strapping pin. GPIO6/7 are plain, unencumbered
// GPIOs — free alongside motors on GPIO1/5, flash on 11-17, USB on 18/19.
#define COMPASS_SDA_PIN   6
#define COMPASS_SCL_PIN   7
#define COMPASS_READ_MS   50    // I2C read/update rate

// Heading-hold PID (yaw correction, output fraction of full yaw authority).
// Conservative starting point — tune on the water.
#define PID_KP               0.02f
#define PID_KI               0.0f
#define PID_KD               0.01f
#define PID_MAX_CORRECTION   0.3f   // clamp: never out-muscle manual steering
#define HEADING_YAW_DEADBAND 0.05f  // |yaw| below this counts as "centered"
// Whether the compass's derived heading increases when the boat turns right
// depends on how the module is physically mounted — can't be known until it's
// bench-tested (twist the board by hand, watch the printed correction). Flip
// to -1 if the correction reinforces drift instead of opposing it.
#define PID_SIGN             1

// Default settings (overridden by whatever's saved in boat Preferences)
#define DEFAULT_MIN_RUN_US        1150  // throttle response floor (µs)
#define DEFAULT_HEADING_HOLD_ON   0     // stays off until compass is calibrated

// Telemetry cadence while a compass calibration is in progress (ms) — faster
// than the normal TELEMETRY_MS so the phone's progress bar feels live.
#define TELEMETRY_CAL_MS  300
