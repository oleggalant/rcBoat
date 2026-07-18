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
