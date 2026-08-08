#pragma once
#include <stdint.h>

// Shared ESP-NOW link layer for transmitter and boat.
// Pairing: TX broadcasts PKT_DISCOVER; boat answers PKT_DISCOVER_ACK and both
// lock to unicast (MAC-layer ACK + retries — matters at the edge of range).
// The boat also treats a valid PKT_CONTROL from an unknown MAC as implicit
// pairing so it recovers from a mid-session reboot without TX involvement.

// Bring up WiFi STA + ESP-NOW on ESPNOW_CHANNEL (LR PHY if ESPNOW_USE_LR).
// The boat additionally enables the promiscuous-mode RSSI snoop.
void espnowLinkInit(bool isBoat);

// Call every loop() iteration on both roles: completes pairing requested from
// the receive callback (peer add / discovery ACK are deferred off the WiFi task).
void espnowLinkService();

bool espnowLinkPaired();
void espnowLinkResetPairing();          // TX: drop peer, return to discovery
uint32_t espnowLinkLastRxMs();          // millis() of last valid packet from peer

// ── Transmitter role ─────────────────────────────────────────────────────────
void espnowLinkSendDiscover();
bool espnowLinkSendControl(int16_t x, int16_t y);
bool espnowLinkSendSettings(uint16_t minRunUs, bool headingHoldEnabled);
bool espnowLinkSendCalCommand(uint8_t cmd);   // CAL_START / CAL_SAVE / CAL_CANCEL
// True if a telemetry packet arrived since the last call; outputs latest values.
bool espnowLinkGetTelemetry(int8_t& rssi, uint8_t& lossPct, int16_t& headingDeg,
                             uint8_t& calState, uint8_t& calCoveragePct,
                             int16_t& rawX, int16_t& rawY);

// ── Boat role ────────────────────────────────────────────────────────────────
// True if a control packet arrived since the last call; outputs latest values.
bool espnowLinkGetControl(int16_t& x, int16_t& y);
uint32_t espnowLinkLastControlMs();
bool espnowLinkSendTelemetry(int8_t rssi, uint8_t lossPct, int16_t headingDeg,
                              uint8_t calState, uint8_t calCoveragePct,
                              int16_t rawX, int16_t rawY);
int8_t espnowLinkLastRxRssi();          // dBm of last frame from the peer
uint8_t espnowLinkTakeLossPct();        // control loss % since previous call
// True if a settings/calibration command arrived since the last call.
bool espnowLinkGetSettings(uint16_t& minRunUs, bool& headingHoldEnabled);
bool espnowLinkGetCalCommand(uint8_t& cmd);
