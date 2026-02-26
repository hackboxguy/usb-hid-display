#include "tusb.h"
#include "pico/unique_id.h"
#include <stdio.h>

// Firmware version as BCD for USB device descriptor (set by CMake)
#ifndef USB_BCD_DEVICE
#define USB_BCD_DEVICE 0x0000
#endif

// Product strings for runtime orientation (selected by g_portrait flag)
#define PRODUCT_STRING_LANDSCAPE "USB HID Display (landscape)"
#define PRODUCT_STRING_PORTRAIT  "USB HID Display (portrait)"

// HID Report Descriptor for Mouse
uint8_t const desc_hid_report[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (Button 1)
    0x29, 0x03,        //     Usage Maximum (Button 3)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x03,        //     Report Count (3)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5)
    0x81, 0x01,        //     Input (Constant) - Reserved 5 bits
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x06,        //     Input (Data, Variable, Relative)
    0xC0,              //   End Collection
    0xC0               // End Collection
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

// Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x1209,        // pid.codes VID
    .idProduct          = 0x0001,        // Example PID
    .bcdDevice          = USB_BCD_DEVICE,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

// Configuration Descriptor
enum {
    ITF_NUM_HID = 0,
    ITF_NUM_CDC_0,
    ITF_NUM_CDC_0_DATA,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // Interface descriptor, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_MOUSE, sizeof(desc_hid_report), 0x81, 16, 10),

    // Interface number, string index, EP notification address and size, EP data address and size
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, 0x82, 8, 0x03, 0x84, 64)
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void)index; // for multiple configurations
    return desc_configuration;
}

// Unique serial number derived from chip flash ID (hex, 16 chars + null)
static char usb_serial_str[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1] = "uninitialized";
static bool serial_initialized = false;

static const char* get_usb_serial(void) {
    if (!serial_initialized) {
        pico_get_unique_board_id_string(usb_serial_str, sizeof(usb_serial_str));
        serial_initialized = true;
    }
    return usb_serial_str;
}

// String Descriptors
// Array of pointer to string descriptors
// Note: index 3 (serial) is handled specially in tud_descriptor_string_cb
char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },       // 0: Supported language is English (0x0409)
    "hackboxguy",                         // 1: Manufacturer
    NULL,                                // 2: Product (resolved at runtime from g_portrait)
    NULL,                                // 3: Serial (populated at runtime from chip ID)
    "CDC Serial"                         // 4: CDC Interface
};

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    static uint16_t _desc_str[32];
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        if (!(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0]))) return NULL;

        // Index 2 = product (runtime orientation), index 3 = serial (chip ID)
        const char* str;
        extern bool g_portrait;
        if (index == 2) {
            str = g_portrait ? PRODUCT_STRING_PORTRAIT : PRODUCT_STRING_LANDSCAPE;
        } else if (index == 3) {
            str = get_usb_serial();
        } else {
            str = string_desc_arr[index];
        }

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        // Convert ASCII to UTF-16
        for (uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}
