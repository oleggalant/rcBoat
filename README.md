# rcBoatEspNow

Long-range RC boat control. Replaces the WiFi/WebSocket link of the original
`rcBoat` project with a dedicated radio hop:

```
Phone (Chrome, Web Bluetooth joystick) → BLE → transmitter ESP32-C3
      → ESP-NOW (Long-Range PHY) → boat ESP32-C3 → dual ESCs
```

## Build & flash

Two firmwares, one per board (both ESP32-C3-DevKitM-1):

```bash
pio run -e boat -t upload
pio run -e transmitter -t upload
pio device monitor            # 115200 baud
```

With both boards plugged in at once, pass `--upload-port COMx` / `--monitor-port COMx`.

## Joystick page (`web/joystick.html`)

Web Bluetooth requires HTTPS, so host the file on any HTTPS static host
(GitHub Pages is the easy path) and open it in Chrome on Android. Tap
**Connect** and pick `rcBoatTx`. No internet is needed after the page loads —
keep the tab open on the water.

## Radio settings (`include/config.h`)

- `ESPNOW_CHANNEL` — must match on both boards.
- `ESPNOW_USE_LR` — 1 enables Espressif Long-Range PHY (~2–4× range; both
  boards must agree or the link is silently dead). Set 0 to A/B-test range.

## Failsafes

- Boat cuts motors to idle if no control packet for 500 ms (`WATCHDOG_MS`).
- Transmitter sends neutral if the phone disconnects or stops writing for
  1 s (`BLE_WATCHDOG_MS`) — the page heartbeats every 250 ms.
- Transmitter re-enters discovery if the boat is silent for 5 s.
- ESCs are held at 1000 µs for 3 s at boot (arming), same as the original.
