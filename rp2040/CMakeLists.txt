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

# Display orientation: "landscape" (default) or "portrait"
set(DISPLAY_ORIENTATION "landscape" CACHE STRING "Display orientation: landscape or portrait")
set_property(CACHE DISPLAY_ORIENTATION PROPERTY STRINGS landscape portrait)

if(DISPLAY_ORIENTATION STREQUAL "portrait")
    message(STATUS "Building with PORTRAIT orientation")
    target_compile_definitions(usb_hid_display PRIVATE DISPLAY_PORTRAIT=1)
elseif(DISPLAY_ORIENTATION STREQUAL "landscape")
    message(STATUS "Building with LANDSCAPE orientation (default)")
else()
    message(FATAL_ERROR "Invalid DISPLAY_ORIENTATION '${DISPLAY_ORIENTATION}'. Must be 'landscape' or 'portrait'.")
endif()

# Firmware version: "X.Y.Z" where X, Y, Z are single digits (0-9)
set(FIRMWARE_VERSION "0.0.0" CACHE STRING "Firmware version in X.Y.Z format (each digit 0-9)")

# Validate version format
if(NOT FIRMWARE_VERSION MATCHES "^[0-9]\\.[0-9]\\.[0-9]$")
    message(FATAL_ERROR "Invalid FIRMWARE_VERSION '${FIRMWARE_VERSION}'. Must be X.Y.Z where each is a single digit (0-9).")
endif()

# Parse version into major, minor, patch and build BCD value
string(REPLACE "." ";" VERSION_PARTS "${FIRMWARE_VERSION}")
list(GET VERSION_PARTS 0 VERSION_MAJOR)
list(GET VERSION_PARTS 1 VERSION_MINOR)
list(GET VERSION_PARTS 2 VERSION_PATCH)
set(BCD_HEX "${VERSION_MAJOR}${VERSION_MINOR}${VERSION_PATCH}0")
message(STATUS "Firmware version: ${FIRMWARE_VERSION} (bcdDevice=0x${BCD_HEX})")

# Pass version and orientation to USB descriptors
target_compile_definitions(usb_hid_display PRIVATE
    USB_BCD_DEVICE=0x${BCD_HEX}
    USB_ORIENTATION_STR="${DISPLAY_ORIENTATION}"
)

# Test/debug commands for automated testing (off by default)
option(ENABLE_TEST_COMMANDS "Enable test/debug commands for automated testing" OFF)
if(ENABLE_TEST_COMMANDS)
    message(STATUS "Test commands ENABLED (0xF0 subcommands)")
    target_compile_definitions(usb_hid_display PRIVATE ENABLE_TEST_COMMANDS)
endif()

# Enable USB output, disable UART output
pico_enable_stdio_usb(usb_hid_display 0)
pico_enable_stdio_uart(usb_hid_display 0)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(usb_hid_display)

# Add include directory
target_include_directories(usb_hid_display PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
#target_include_directories(usb_hid_display PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
