# usb-hid-display

A firmware for Raspberry Pi Pico (RP2040) that acts as a USB composite device: a rotary encoder + directional buttons as HID mouse input, and a `/dev/ttyACMx` serial interface for controlling an SSD1306 OLED display.

![Photos.](/images/photos.jpg "Photos.")

## Features

- USB HID Mouse: rotary encoder for horizontal movement, directional buttons for X/Y, push button for left-click
- USB CDC Serial: binary command protocol to draw text, progress bars, control brightness/power/inversion on the SSD1306
- Composite USB device: both interfaces available simultaneously over a single USB connection
- Robust I2C communication with timeouts (no hangs if display disconnects)
- Proper quadrature decoding with Gray code for reliable rotary input
- Per-button debouncing for all inputs
- Unique USB serial number derived from RP2040 chip ID

## Hardware Requirements

- Raspberry Pi Pico (RP2040)
- SSD1306 OLED display (128x64, I2C version)
- Rotary encoder with push button
- 4x directional push buttons (active-low, directly connected to GPIO with internal pull-ups)
- Micro USB cable

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
| CLK (A)    | GPIO 27 | Pin 32   |
| DT (B)     | GPIO 26 | Pin 31   |
| SW (Enter) | GPIO 14 | Pin 19   |
| VCC        | 3.3V    | Pin 36   |
| GND        | GND     | Pin 38   |

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

**Note:** Each directional button press generates two HID movement events (one immediate, one after ~16ms) to match rotary encoder step responsiveness. Host-side daemons should account for this when interpreting navigation input.

## Serial Command Protocol

The device exposes a CDC serial port (`/dev/ttyACMx`) that accepts binary commands to control the display:

| Command | Code | Format | Description |
|---------|------|--------|-------------|
| Clear   | `0x01` | `[0x01]` | Clear entire display |
| Draw Text | `0x02` | `[0x02][x][y][text...]` | Draw text at pixel position (x, y). Text Y is 8-pixel aligned (page-based): use multiples of 8 (0, 8, 16, 24, 32, 40, 48, 56) |
| Set Cursor | `0x03` | `[0x03][x][y]` | Set cursor to pixel position (x, y) |
| Invert  | `0x04` | `[0x04][0/1]` | Normal or inverted display mode |
| Brightness | `0x05` | `[0x05][0-255]` | Set display contrast/brightness |
| Progress Bar | `0x06` | `[0x06][x][y][w][h][0-100]` | Draw progress bar at (x, y) with width, height, and percentage |
| Power   | `0x07` | `[0x07][0/1]` | Turn display off or on |

### Example (Python)

```python
import serial

ser = serial.Serial('/dev/ttyACM0', timeout=1)

# Clear display
ser.write(bytes([0x01]))

# Draw text "Hello" at position (0, 0)
ser.write(bytes([0x02, 0, 0]) + b'Hello')

# Draw progress bar at (10, 30), 108px wide, 12px tall, 75%
ser.write(bytes([0x06, 10, 30, 108, 12, 75]))

# Set brightness to max
ser.write(bytes([0x05, 255]))
```

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
cmake ..
make -j$(nproc)
```

#### Portrait orientation build

To build for portrait mode (buttons rotated 90°, text renders bottom-to-top):

```bash
cmake .. -DDISPLAY_ORIENTATION=portrait
make -j$(nproc)
```

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
echo -ne '\x02\x00\x00Hello' > /dev/ttyACM0  # draw "Hello" at (0,0)
```

## USB Device Info

| Field | Value |
|-------|-------|
| Vendor ID  | `0x1209` (pid.codes) |
| Product ID | `0x0001` |
| Manufacturer | DIY Projects |
| Product | Pico Encoder Display |
| Serial | Unique per chip (from RP2040 flash ID) |

## License

See [LICENSE](LICENSE) for details.
