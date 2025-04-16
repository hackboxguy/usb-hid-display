cmake_minimum_required(VERSION 3.13)

# Initialize the SDK
include(pico_sdk_import.cmake)

project(usb_hid_display C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()
# Add executable
add_executable(usb_hid_display
    src/main.cpp
    src/rotary_encoder.cpp
    src/ssd1306.cpp
    src/usb_descriptors.c
)

# Pull in commonly used features
target_link_libraries(usb_hid_display
    pico_stdlib
    hardware_i2c
    pico_unique_id
    tinyusb_device
    tinyusb_board
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(usb_hid_display 0)
pico_enable_stdio_uart(usb_hid_display 0)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(usb_hid_display)

# Add include directory
target_include_directories(usb_hid_display PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
#target_include_directories(usb_hid_display PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
