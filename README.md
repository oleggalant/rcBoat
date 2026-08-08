# rcBoat — a 3D-printed RC catamaran, build it yourself

A weekend (or two) DIY project: a long-range radio-controlled boat with a
3D-printed hull and **four empty cola bottles** as floats. build it yourself! it only takes one weekend on the hull and floats, one
weekend on the electronics and the first drive.

```
Phone (Chrome, Web Bluetooth joystick) → BLE → transmitter ESP32-C3
      → ESP-NOW (Long-Range PHY) → boat ESP32-C3 → dual ESCs
```

## What you need

- A 3D printer (STL files: see [`stls/`](stls/))
- 4 empty 1.5–2 L cola bottles, rinsed out, labels off, caps on
- 2x ESP32-C3-DevKitM-1 boards (one for the boat, one for the handheld
  transmitter) — the transmitter board should have an external antenna
- 2x brushless motors + ESCs, propellers
- A GY-271 compass module for the boat, for heading-hold (see chip note below)
- A phone with Chrome (Android or desktop) for the controller page
- Small Li-ion/LiPo or USB power bank for the transmitter; a battery + BEC
  (or separate 5V supply) for the boat electronics

## Weekend 1 — hull & floats

Print the hull parts, mount the bottles as floats, fit the motor/ESC
mounts:

- [`stls/`](stls/) — STL files for the printed parts (motor mount, bottle
  T-connector, turnbuckle)
- [`photos/`](photos/) — build photos and reference pictures

*(Photos to follow.)*

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
- good for fishing if phone held in one hand,fishing rod in other
- **2-hand**: landscape, turn slider on the left, thrust slider on the right
  — good for racing/casual sailing and water watching(needs an external camera)

### Compass wiring (GY-271, boat only)

| GY-271 pin | ESP32-C3 pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL | GPIO7 |
| SDA | GPIO6 |
| DRDY / INT | not connected |

GPIO8/9 are deliberately avoided: GPIO8 is hardwired to the DevKitM-1's
onboard addressable RGB LED (which can interfere with I2C on that pin), and
GPIO9 is a boot-strapping pin — GPIO6/7 are plain, unencumbered GPIOs.

Mount it as far as practical from the ESCs and motor wiring — motor current
is the most likely source of magnetic interference — and roughly level.

**Chip note**: boards sold as "GY-271 QMC5883L" increasingly ship a newer,
pin-similar chip called **QMC5883P** instead — same footprint, different I2C
address (`0x2C` vs the older `0x0D`) and register map. This firmware targets
QMC5883P (via the `Adafruit_QMC5883P` library). If your board turns out to
have a genuine QMC5883L or an HMC5883L clone instead, flash
`pio run -e compass_test -t upload` and open `pio device monitor` — it scans
the whole I2C bus, fingerprints whichever of the three chips is present
(chip-ID register match, not just an address guess), and streams live raw
X/Y/Z once identified, all without the rest of the radio stack running.

**Wiring debug on the water**: `web/trace.html` (also linked as "Trace" from
the main controller page) connects over the same BLE link and shows a live
readout — RSSI, heading, calibration state, raw compass X/Y, and which I2C
address last ACKed — plus a scrolling log of every telemetry update, with no
serial cable needed.

### Settings panel (⚙ button on the joystick page)

- **Heading Hold** — while driving forward with the turn stick centered, the
  boat locks onto its current heading and corrects for drift (prop torque,
  wind, current) — like an RC "heading-hold gyro". Turning always overrides
  it immediately; releasing the stick locks onto the new heading. Requires
  the compass to be calibrated first.
- **Min throttle PWM** — a response floor: any nonzero throttle jumps
  straight to this PWM instead of crawling up from 1000 µs, where the prop
  may not even be spinning yet.
- **Calibrate Compass** — rotate the boat through a full circle (a figure-8
  works well) for about 20 seconds, away from other metal/magnets, then tap
  Save. The progress bar fills as it sees more of the circle. Calibration is
  stored on the boat and survives reboots.

If heading-hold ever fights a turn instead of helping it, the compass's
mounting orientation has the sign backwards for this particular module —
flip `PID_SIGN` in `include/config.h` from `1` to `-1` and reflash the boat.

### Radio settings (`include/config.h`)

- `ESPNOW_CHANNEL` — must match on both boards.
- `ESPNOW_USE_LR` — 1 enables Espressif Long-Range PHY (~2–4× range; both
  boards must agree or the link is silently dead). Set 0 to A/B-test range.

### Failsafes

- Boat cuts motors to idle if no control packet is recived for 500 ms (`WATCHDOG_MS`).
- Transmitter sends neutral signals if the phone disconnects or stops writing for
  1 s (`BLE_WATCHDOG_MS`) — the page heartbeats every 250 ms.
- Transmitter re-enters discovery if the boat is silent for 5 s.
- ESCs are held at 1000 µs for 3 s at boot (arming), same as the original.

## Safety notes (worth a read )

- First runs: **props off**. Verify pairing, joystick response, and that
  killing the transmitter or closing the phone tab actually stops the boat.
- Keep hands clear of props once they're on — arm the ESCs, then don't touch.
- Test failsafes deliberately: turn off the transmitter, close the browser
  tab, walk out of BLE range — in every case the boat should stop.
- Water testing: !carefully! calm/shallow water first, and a plan for
  retrieving the boat if the link ever does drop.
