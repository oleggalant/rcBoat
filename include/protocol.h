#pragma once
#include <stdint.h>
#include <stddef.h>

// ESP-NOW wire protocol between transmitter and boat.
// Magic/version/CRC exist to reject foreign ESP-NOW traffic that happens to
// reach our receive callback — ESP-NOW itself already CRCs at the MAC layer.

#define PROTO_MAGIC   0xB7
#define PROTO_VERSION 2

enum : uint8_t {
    PKT_CONTROL      = 1,   // TX -> boat: joystick values
    PKT_TELEMETRY    = 2,   // boat -> TX: link quality + compass status
    PKT_DISCOVER     = 3,   // TX -> broadcast: looking for a boat
    PKT_DISCOVER_ACK = 4,   // boat -> TX unicast: pair with me
    PKT_SETTINGS     = 5,   // TX -> boat: throttle floor / heading-hold toggle
    PKT_CAL_CMD      = 6,   // TX -> boat: compass calibration start/save/cancel
};

enum : uint8_t {
    CAL_START  = 1,
    CAL_SAVE   = 2,
    CAL_CANCEL = 3,
};

enum : uint8_t {
    CAL_STATE_IDLE        = 0,
    CAL_STATE_CALIBRATING = 1,
    CAL_STATE_SAVED       = 2,
};

typedef struct __attribute__((packed)) {
    uint8_t magic;          // PROTO_MAGIC
    uint8_t version;        // PROTO_VERSION
    uint8_t type;           // PKT_*
    uint8_t seq;            // wraps; boat counts gaps for loss stats
} PacketHeader;

typedef struct __attribute__((packed)) {
    PacketHeader hdr;       // type = PKT_CONTROL
    int16_t x;              // yaw      * 1000 (±1200 — trim can exceed ±1000)
    int16_t y;              // throttle * 1000 (±1000)
    uint8_t crc;            // CRC-8 over all preceding bytes
} ControlPacket;            // 9 bytes

typedef struct __attribute__((packed)) {
    PacketHeader hdr;       // type = PKT_TELEMETRY
    int8_t  rssi;           // dBm of last control packet seen at the boat
    uint8_t lossPct;        // control-packet loss % over the last window
    int16_t headingDeg;     // 0-359, or -1 if compass not calibrated
    uint8_t calState;       // CAL_STATE_*
    uint8_t calCoveragePct; // calibration progress, 0-100 (0 outside calibration)
    uint8_t crc;
} TelemetryPacket;          // 11 bytes

typedef struct __attribute__((packed)) {
    PacketHeader hdr;       // type = PKT_DISCOVER or PKT_DISCOVER_ACK
    uint8_t crc;
} DiscoveryPacket;          // 5 bytes

typedef struct __attribute__((packed)) {
    PacketHeader hdr;       // type = PKT_SETTINGS
    uint16_t minRunUs;          // throttle response floor (µs)
    uint8_t  headingHoldEnabled;
    uint8_t  crc;
} SettingsPacket;           // 8 bytes

typedef struct __attribute__((packed)) {
    PacketHeader hdr;       // type = PKT_CAL_CMD
    uint8_t cmd;            // CAL_START / CAL_SAVE / CAL_CANCEL
    uint8_t crc;
} CalCommandPacket;         // 6 bytes

// CRC-8 Dallas/Maxim (poly 0x31 reflected)
static inline uint8_t proto_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    while (len--) {
        uint8_t b = *data++;
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t mix = (crc ^ b) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            b >>= 1;
        }
    }
    return crc;
}
