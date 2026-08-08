#pragma once
#include <stdint.h>

// NimBLE peripheral exposing a Nordic-UART-derived service:
//   NUS RX   (write):  4 bytes  — int16 x, int16 y (LE, x1000) — joystick, ~20 Hz
//   Settings (write):  3 bytes  — uint16 minRunUs (LE), uint8 headingHoldEnabled
//   Cal cmd  (write):  1 byte   — CAL_START/CAL_SAVE/CAL_CANCEL (see protocol.h)
//   NUS TX   (notify): JSON telemetry, e.g.
//     {"rssi":-62,"heading":137,"cal":0,"calPct":0,"rawX":120,"rawY":-340,"i2c":44}
//   i2c: 44 (0x2C) = QMC5883P (supported), 13 (0x0D) = QMC5883L, 30 (0x1E) =
//        HMC5883L-clone address (both unsupported by this firmware), 0 = nothing found
// Settings/cal get their own characteristics rather than sharing NUS RX so a
// one-shot write never has to share framing with the high-rate control path.
void bleUartInit();

bool bleUartConnected();

// True if a new joystick write arrived since the last call; outputs latest values.
bool bleUartGetControl(int16_t& x, int16_t& y);
uint32_t bleUartLastWriteMs();          // 0 = never

// True if a new settings/calibration write arrived since the last call.
bool bleUartGetSettings(uint16_t& minRunUs, bool& headingHoldEnabled);
bool bleUartGetCalCommand(uint8_t& cmd);

void bleUartNotifyTelemetry(int8_t rssi, int16_t headingDeg, uint8_t calState,
                             uint8_t calCoveragePct, int16_t rawX, int16_t rawY,
                             uint8_t i2cAddr);
