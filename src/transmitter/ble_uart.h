#pragma once
#include <stdint.h>

// NimBLE peripheral exposing the Nordic UART Service (NUS).
//   RX characteristic: phone writes 4 bytes — int16 x, int16 y (LE, ×1000)
//   TX characteristic: notifies telemetry JSON, e.g. {"rssi":-62}
void bleUartInit();

bool bleUartConnected();

// True if a new write arrived since the last call; always outputs latest values.
bool bleUartGetControl(int16_t& x, int16_t& y);
uint32_t bleUartLastWriteMs();          // 0 = never

void bleUartNotifyRssi(int8_t rssi);
