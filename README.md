# usb-hid-display
A firmware for rpi-pico to acts as an usb-composite-device(rotary-encoder as hid-iput and /dev/ttyACMx serial device for controlling SSD1306)

# Hardware Requirements
- Raspberry Pi Pico (~$4-6)
- Rotary encoder with push button
- SSD1306 OLED display (I2C version)
- Breadboard and jumper wires
- Micro USB cable

# Hardware Connections
## SSD1306 OLED Display(I2C)
- VCC → 3.3V (Pin 36 on Pico)
- GND → GND (Pin 38 on Pico)
- SCL → GPIO 5 (Pin 7 on Pico)
- SDA → GPIO 4 (Pin 6 on Pico)
## Rotary Encoder
- CLK (A) → GPIO 10 (Pin 14 on Pico)
- DT (B) → GPIO 11 (Pin 15 on Pico)
- SW (Button) → GPIO 12 (Pin 16 on Pico)
- GND → GND (Pin 33 on Pico)
- VCC → 3.3V (Pin 36 on Pico)
## Directional push buttons(pull down to GND)
- LEFT  → GPIO 6 of rp2040
- RIGHT → GPIO 7 of rp2040
- TOP   → GPIO 9 of rp2040
- BOT   → GPIO 8 of rp2040

# How to build firmware for rp2040
- ```sudo apt update```
- ```sudo apt install git cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential libstdc++-arm-none-eabi-newlib```
- ```cd ~/```
- ```git clone --recursive https://github.com/raspberrypi/pico-sdk.git```
- ```export PICO_SDK_PATH=~/pico-sdk```
- ```git clone https://github.com/hackboxguy/usb-hid-display.git```
- ```cd usb-hid-display/rp2040/```
- ```git clone https://github.com/hathach/tinyusb.git```
- ```mkdir build```
- ```cd build```
- ```cmake ..```
- ```make -j$(nproc)```
- Connect rp2040 to usb while holding BOOTSEL button
- ```dmesg``` (check for the rp2040 node /dev/sdxN)
- ```sudo mount /dev/sdxN /mnt/rp2040```
- ```sudo cp usb_hid_display.uf2 /mnt/rp2040/```
- ```sudo umount /mnt/rp2040```
- Now rp2040 should show up as /dev/input/eventX and /dev/ttyACMX
- Test events using ```sudo evtest /dev/input/eventx```
