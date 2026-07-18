#pragma once
#include <stdint.h>
#include <stddef.h>

// ESP-NOW wire protocol between transmitter and boat.
// Magic/version/CRC exist to reject foreign ESP-NOW traffic that happens to
// reach our receive callback — ESP-NOW itself already CRCs at the MAC layer.

#define PROTO_MAGIC   0xB7
#define PROTO_VERSION 1

enum : uint8_t {
    PKT_CONTROL      = 1,   // TX -> boat: joystick values
    PKT_TELEMETRY    = 2,   // boat -> TX: link quality
    PKT_DISCOVER     = 3,   // TX -> broadcast: looking for a boat
    PKT_DISCOVER_ACK = 4,   // boat -> TX unicast: pair with me
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
    uint8_t crc;
} TelemetryPacket;          // 7 bytes

typedef struct __attribute__((packed)) {
    PacketHeader hdr;       // type = PKT_DISCOVER or PKT_DISCOVER_ACK
    uint8_t crc;
} DiscoveryPacket;          // 5 bytes

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
