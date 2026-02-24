# usb-hid-display

A firmware for Raspberry Pi Pico (RP2040) that turns cheap off-the-shelf components (rotary encoder, push buttons, SSD1306 OLED) into a standalone USB micro-panel for embedded Linux devices.

![RP2040-Micropanel](/images/finished-pcb.gif "RP2040-Micropanel 3D View")

## What is this?

This firmware creates a USB composite device with two interfaces:

- **HID Mouse** — The rotary encoder and directional buttons appear as a standard USB mouse to the host. A host-side daemon (such as [micropanel](https://github.com/nicupavel/micropanel)) interprets the relative mouse movements as menu navigation: up, down, left, right, and select.
- **CDC Serial** (`/dev/ttyACMx`) — The host writes binary commands over this serial port to control the SSD1306 OLED display: draw text, progress bars, set brightness, etc.

Together, these two interfaces form a self-contained hardware menu system — physical buttons for input, OLED for output — driven entirely from the host over a single USB cable. No drivers required; the device uses standard USB HID and CDC ACM classes supported by all major operating systems.

## Features

- USB HID Mouse: rotary encoder for horizontal movement, directional buttons for X/Y navigation, push button for left-click/select
- USB CDC Serial: binary command protocol to draw text, progress bars, control brightness/power/inversion on the SSD1306
- Single USB connection: both HID and CDC interfaces available simultaneously
- Robust I2C communication with timeouts (no hangs if display disconnects)
- Proper quadrature decoding with Gray code for reliable rotary input
- Per-button debouncing for all inputs
- Unique USB serial number derived from RP2040 chip ID
- Runtime landscape/portrait orientation via GPIO jumper (single firmware binary)

## Hardware Variants

### Custom PCB — RP2040-Micropanel

A purpose-built PCB designed in KiCad with a Waveshare RP2040-Zero module, SSD1306 OLED, and 5 tactile buttons on a compact board. KiCad project files are available in the [`hardware/`](hardware/) directory.

![RP2040-Micropanel PCB](/images/finished-pcb-collage.jpg "RP2040-Micropanel: KiCad renders and assembled board")

### Prototype — Hand-Soldered

The same firmware runs on any RP2040 board with hand-wired connections on a general-purpose PCB. No custom PCB required — see the wiring diagram in the [Directional Push Buttons](#directional-push-buttons-active-low-directly-connected-to-gnd) section below.

![Prototype builds](/images/photos.jpg "Various prototype builds with hand-soldered connections")

## Hardware Requirements

- Raspberry Pi Pico (RP2040)
- SSD1306 OLED display (128x64, I2C version)
- Rotary encoder with push button
- 4x directional push buttons (active-low, directly connected to GPIO with internal pull-ups)
- Micro USB cable
- Optional: jumper wire for portrait mode (GPIO 27 to GND)

## Hardware Connections

### SSD1306 OLED Display (I2C)

| Signal | GPIO | Pico Pin |
|--------|------|----------|
| SDA    | GPIO 4 | Pin 6 |
| SCL    | GPIO 5 | Pin 7 |
| VCC    | 3.3V | Pin 36 |
| GND    | GND  | Pin 38 |

### Rotary Encoder

| Signal     | GPIO    | Pico Pin |
|------------|---------|----------|
| CLK (A)    | GPIO 10 | Pin 14   |
| DT (B)     | GPIO 11 | Pin 15   |
| SW (Button)| GPIO 12 | Pin 16   |
| VCC        | 3.3V    | Pin 36   |
| GND        | GND     | Pin 38   |

### Orientation Jumper

The firmware detects landscape or portrait mode at boot from GPIO 27:

| GPIO 27 State | Orientation |
|---------------|-------------|
| Floating (internal pull-up → HIGH) | **Landscape** (default) |
| Connected to GND (LOW) | **Portrait** |

No separate firmware builds needed — a single `.uf2` handles both orientations. The detected orientation is reflected in the USB product string (`"USB HID Display (landscape)"` or `"USB HID Display (portrait)"`).

### Directional Push Buttons (active-low, directly connected to GND)

Pre-compiled `.uf2` binary with this GPIO config can be downloaded from [here](https://github.com/hackboxguy/usb-hid-display/blob/main/rp2040/bin/usb_hid_display.uf2?raw=true).

![Buttons-Wiring.](/images/buttons-wiring.jpg "Buttons-Wiring.")

The table below shows the **landscape orientation** (default) mapping. In portrait mode, the physical button roles rotate: LEFT becomes UP, RIGHT becomes DOWN, DOWN becomes LEFT, UP becomes RIGHT.

| Button (Landscape) | GPIO    | HID Action     |
|---------------------|---------|----------------|
| LEFT   | GPIO 7  | Mouse X = -5   |
| RIGHT  | GPIO 6  | Mouse X = +5   |
| UP     | GPIO 8  | Mouse Y = -5   |
| DOWN   | GPIO 15 | Mouse Y = +5   |
| ENTER  | GPIO 14 | Mouse Left Click (shared with rotary SW) |

Portrait/landscape mapping summary:

| Physical Button | Landscape Action | Portrait Action |
|-----------------|------------------|-----------------|
| LEFT (GPIO 7)   | Left             | Up              |
| RIGHT (GPIO 6)  | Right            | Down            |
| UP (GPIO 8)     | Up               | Right           |
| DOWN (GPIO 15)  | Down             | Left            |
| ENTER (GPIO 14) | Select (BTN_LEFT)| Select (BTN_LEFT) |

**Note:** Each directional button press generates two HID movement events (one immediate, one after ~16ms) to match rotary encoder step responsiveness. Host-side daemons should account for this when interpreting navigation input.

## Serial Command Protocol

The device exposes a CDC serial port (`/dev/ttyACMx`) that accepts binary commands to control the display:

| Command | Code | Format | Description |
|---------|------|--------|-------------|
| Clear   | `0x01` | `[0x01]` | Clear entire display |
| Draw Text | `0x02` | `[0x02][x][y][len][text...]` | Draw `len` bytes of text at pixel position (x, y). Max len=124. Text Y is 8-pixel aligned (page-based): use multiples of 8 (0, 8, 16, 24, 32, 40, 48, 56) |
| Set Cursor | `0x03` | `[0x03][x][y]` | Set cursor to pixel position (x, y) |
| Invert  | `0x04` | `[0x04][0/1]` | Normal or inverted display mode |
| Brightness | `0x05` | `[0x05][0-255]` | Set display contrast/brightness |
| Progress Bar | `0x06` | `[0x06][x][y][w][h][0-100]` | Draw progress bar at (x, y) with width, height, and percentage |
| Power   | `0x07` | `[0x07][0/1]` | Turn display off or on |

### Protocol Limits and Caveats

- `MAX_CMD_SIZE` is 128 bytes total per command buffer.
- Commands that exceed the buffer are truncated to avoid parser desynchronization.
- `CMD_DRAW_TEXT` uses length-based framing: the `len` byte specifies exactly how many text bytes follow (max 124).
- All commands have deterministic framing — fixed-length (0x01, 0x03-0x07) or length-prefixed (0x02).
- Text Y is page-based (8-pixel rows): use `0, 8, 16, ..., 56`.

### Example (Python)

```python
import serial

ser = serial.Serial('/dev/ttyACM0', timeout=1)

# Clear display
ser.write(bytes([0x01]))

# Draw text "Hello" at position (0, 0) — len=5
ser.write(bytes([0x02, 0, 0, 5]) + b'Hello')

# Draw progress bar at (10, 30), 108px wide, 12px tall, 75%
ser.write(bytes([0x06, 10, 30, 108, 12, 75]))

# Set brightness to max
ser.write(bytes([0x05, 255]))
```

## Test Commands (optional, build-time enabled)

When built with `-DENABLE_TEST_COMMANDS=ON`, the firmware accepts command `0xF0` for automated hardware testing. This allows the host test framework to inject simulated HID input events through the CDC serial port without physically pressing buttons.

Production firmware ignores `0xF0` entirely.

| Subcommand | Code | Action | HID Output |
|------------|------|--------|------------|
| Ping | `[0xF0][0x00]` | Replies `[0xF0][0x00]` via CDC | None |
| Rotate CW | `[0xF0][0x01]` | One clockwise encoder step | REL_X = -5 |
| Rotate CCW | `[0xF0][0x02]` | One counter-clockwise step | REL_X = +5 |
| Button press | `[0xF0][0x03]` | Encoder push (press + release) | BTN_LEFT down, then up after ~50ms |
| Navigate up | `[0xF0][0x04]` | Simulated UP button | 2x REL_Y = -5 (~16ms apart) |
| Navigate down | `[0xF0][0x05]` | Simulated DOWN button | 2x REL_Y = +5 (~16ms apart) |
| Navigate left | `[0xF0][0x06]` | Simulated LEFT button | 2x REL_X = -5 (~16ms apart) |
| Navigate right | `[0xF0][0x07]` | Simulated RIGHT button | 2x REL_X = +5 (~16ms apart) |

Navigation test commands produce orientation-independent logical directions — the same HID output regardless of landscape/portrait build.

**Timing:** Allow at least 60ms between nav commands and 80ms after button press before sending the next test command. The host test framework can detect test firmware by sending ping and waiting 500ms for a reply.

## Building the Firmware

### Prerequisites

```bash
sudo apt update
sudo apt install git cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential libstdc++-arm-none-eabi-newlib
```

### Clone and build

```bash
git clone --recursive https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
export PICO_SDK_PATH=~/pico-sdk

git clone https://github.com/hackboxguy/usb-hid-display.git
cd usb-hid-display/rp2040/
mkdir build && cd build
cmake .. -DFIRMWARE_VERSION=1.0.0
make -j$(nproc)
```

#### Build options

| Option | Default | Description |
|--------|---------|-------------|
| `FIRMWARE_VERSION` | `0.0.0` | Firmware version in `X.Y.Z` format (each digit 0-9) |
| `ENABLE_TEST_COMMANDS` | `OFF` | Enable test/debug commands (`0xF0`) for automated testing |

`FIRMWARE_VERSION` is embedded in USB descriptors and exposed to the host via sysfs during USB enumeration. Orientation is detected at runtime from the GPIO 27 jumper (no build flag needed).

#### Test command build

To build firmware with test commands enabled (for automated hardware testing):

```bash
cmake .. -DENABLE_TEST_COMMANDS=ON -DFIRMWARE_VERSION=1.0.1
make -j$(nproc)
```

Production builds (without `-DENABLE_TEST_COMMANDS=ON`) ignore the test command byte entirely.

### Flashing

1. Hold the **BOOTSEL** button on the Pico and connect it via USB
2. The Pico mounts as a USB mass storage device
3. Copy the firmware:

```bash
# Check dmesg for the device node (e.g. /dev/sda1)
sudo mount /dev/sdXN /mnt
sudo cp usb_hid_display.uf2 /mnt/
sudo umount /mnt
```

After flashing, the device enumerates as:
- `/dev/input/eventX` -- HID mouse (rotary encoder + buttons)
- `/dev/ttyACMX` -- CDC serial port (display commands)

### Verify

```bash
# Test HID input events
sudo evtest /dev/input/eventX

# Test display commands
echo -ne '\x01' > /dev/ttyACM0          # clear display
echo -ne '\x02\x00\x00\x05Hello' > /dev/ttyACM0  # draw "Hello" at (0,0), len=5
```

## USB Device Info

| Field | Value |
|-------|-------|
| Vendor ID  | `0x1209` (pid.codes) |
| Product ID | `0x0001` |
| Manufacturer | hackboxguy |
| Product | `USB HID Display (<orientation>)` — reflects runtime GPIO jumper state |
| Serial | Unique per chip (from RP2040 flash ID) |
| bcdDevice | Firmware version as BCD (e.g. `0x1010` for v1.0.1) |

### Version Encoding

`FIRMWARE_VERSION=X.Y.Z` is encoded as USB `bcdDevice = 0xXYZ0`:

| Version | bcdDevice |
|---------|-----------|
| `1.0.0` | `1000`    |
| `1.0.1` | `1010`    |
| `1.1.0` | `1100`    |

### Reading firmware info from host

The daemon (or any tool) can read firmware version and orientation from sysfs without opening the serial port:

```bash
# Find the device (filter by VID:PID)
DEVPATH=$(grep -rl "1209" /sys/bus/usb/devices/*/idVendor 2>/dev/null | head -1 | xargs dirname)

# Read product string (includes orientation)
cat "$DEVPATH/product"    # e.g. "USB HID Display (portrait)"

# Read firmware version (BCD format)
cat "$DEVPATH/bcdDevice"  # e.g. "1100" for v1.1.0
```

## Host Integration

Use stable paths instead of raw `eventX` / `ttyACM0` indices:

```bash
# serial path
ls -l /dev/serial/by-id/

# input path
ls -l /dev/input/by-id/
```

Optional udev rule for a stable symlink:

```bash
# /etc/udev/rules.d/99-usb-hid-display.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="0001", SYMLINK+="usb-hid-display-%s{serial}"
```

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Troubleshooting

| Symptom | Likely Cause | Check / Fix |
|---------|--------------|-------------|
| No `/dev/ttyACM*` | USB cable or enumeration issue | Try a data-capable cable, check `dmesg -w` |
| No HID events | Wrong `eventX` selected | Use `evtest` and pick matching device from `/dev/input/by-id` |
| Display stays blank | I2C wiring/power/address issue | Verify SDA/SCL pins, 3.3V power, GND, SSD1306 address `0x3C` |
| Buttons feel double-step | Intended dual-event behavior | Account for two events per press in daemon logic |
| Wrong navigation direction | Orientation mismatch | Check GPIO 27 jumper: GND = portrait, floating = landscape |
| Permission denied on device files | User/group access | Add user to `dialout` and `input` groups or use udev permissions |

## License

See [LICENSE](LICENSE) for details.
