# usb-hid-display
A firmware for the mcu to acts as an usb-composite-device(rotary-encoder as hid-iput and ttyACMx serial device for controlling SSD1306)

# How to build firmware for rp2040
- ```cd ~/```
- ```git clone https://github.com/raspberrypi/pico-sdk.git```
- ```export PICO_SDK_PATH=~/pico-sdk```
- ```git clone https://github.com/hackboxguy/usb-hid-display.git```
- ```cd usb-hid-display/rp2040/```
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
