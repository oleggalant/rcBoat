# rcBoat — a 3D-printed RC boat, built with your kids

A weekend (or two) DIY project: a long-range radio-controlled boat with a
3D-printed hull and **four empty cola bottles** as floats. Aimed at building
it together with your children — one weekend on the hull and floats, one
weekend on the electronics and the first drive.

```
Phone (Chrome, Web Bluetooth joystick) → BLE → transmitter ESP32-C3
      → ESP-NOW (Long-Range PHY) → boat ESP32-C3 → dual ESCs
```

## What you need

- A 3D printer (STL files: see [`hull/`](hull/) — coming soon)
- 4 empty 1.5–2 L cola bottles, rinsed out, labels off, caps on
- 2x ESP32-C3-DevKitM-1 boards (one for the boat, one for the handheld
  transmitter) — the transmitter board should have an external antenna
- 2x brushless motors + ESCs, propellers
- A phone with Chrome (Android or desktop) for the controller page
- Small Li-ion/LiPo or USB power bank for the transmitter; a battery + BEC
  (or separate 5V supply) for the boat electronics

## Weekend 1 — hull & floats

Print the hull parts, mount the bottles as floats, fit the motor/ESC
mounts. STL files and build photos go here once ready:

- [`hull/`](hull/) — STL / 3MF files for the printed parts
- [`photos/`](photos/) — build photos and reference pictures

*(Placeholder — files to follow.)*

## Weekend 2 — electronics & first drive

This is the part that's already built and tested. See below.

### Build & flash

Two firmwares, one per board (both ESP32-C3-DevKitM-1):

```bash
pio run -e boat -t upload
pio run -e transmitter -t upload
pio device monitor            # 115200 baud
```

With both boards plugged in at once, pass `--upload-port COMx` / `--monitor-port COMx`.

### Joystick page (`web/joystick.html`)

Web Bluetooth requires HTTPS, so host the file on any HTTPS static host
(GitHub Pages is the easy path) and open it in Chrome. Tap **Connect** and
pick `rcBoatTx`. No internet is needed after the page loads — keep the tab
open on the water.

Two control layouts, switchable in the page (saved per-device):
- **1-stick**: single joystick, horizontal = turn, vertical = thrust
- **2-hand**: landscape, turn slider on the left, thrust slider on the right
  — good for letting one kid drive while sharing the phone, or for small
  hands that find a diagonal joystick harder to hold steady

### Radio settings (`include/config.h`)

- `ESPNOW_CHANNEL` — must match on both boards.
- `ESPNOW_USE_LR` — 1 enables Espressif Long-Range PHY (~2–4× range; both
  boards must agree or the link is silently dead). Set 0 to A/B-test range.

### Failsafes

- Boat cuts motors to idle if no control packet for 500 ms (`WATCHDOG_MS`).
- Transmitter sends neutral if the phone disconnects or stops writing for
  1 s (`BLE_WATCHDOG_MS`) — the page heartbeats every 250 ms.
- Transmitter re-enters discovery if the boat is silent for 5 s.
- ESCs are held at 1000 µs for 3 s at boot (arming), same as the original.

## Safety notes (worth a read before the kids press go)

- First runs: **props off**. Verify pairing, joystick response, and that
  killing the transmitter or closing the phone tab actually stops the boat.
- Keep hands clear of props once they're on — arm the ESCs, then don't touch.
- Test failsafes deliberately: turn off the transmitter, close the browser
  tab, walk out of BLE range — in every case the boat should stop.
- Water testing: adult supervision, calm/shallow water first, and a plan for
  retrieving the boat if the link ever does drop.
