#include "main.h"

// Debug flag - set to false for production use
#define DEBUG_MODE      false

// Buffer for receiving commands from serial
static uint8_t serial_buf[MAX_CMD_SIZE];
static uint8_t serial_buf_pos = 0;

// Timestamp for non-blocking text accumulation (CMD_DRAW_TEXT)
static absolute_time_t text_cmd_start = {0};
static bool text_cmd_pending = false;
#define TEXT_CMD_TIMEOUT_US 5000 // 5ms accumulation window

#ifdef ENABLE_TEST_COMMANDS
// Pending test event for delayed HID reports (button release, second nav event)
static struct {
    bool pending;
    absolute_time_t fire_time;
    uint8_t buttons;
    int8_t x;
    int8_t y;
} test_pending_event = {false, {0}, 0, 0, 0};

#define TEST_BTN_RELEASE_DELAY_US 50000  // 50ms between press and release
#define TEST_NAV_SECOND_EVENT_US  16000  // 16ms between nav events (matches real buttons)

static void handle_test_command(uint8_t subcmd) {
    // Warn if a pending event will be overwritten (debug aid for test timing issues)
    if (test_pending_event.pending && DEBUG_MODE) {
        ssd1306_draw_text(0, 48, "WARN:test evt overwrite");
    }

    switch (subcmd) {
        case TEST_SUBCMD_PING:
        {
            uint8_t reply[2] = {CMD_TEST, TEST_SUBCMD_PING};
            tud_cdc_write(reply, 2);
            tud_cdc_write_flush();
            break;
        }
        case TEST_SUBCMD_ROTATE_CW:
            send_mouse_report(0, -5, 0, 0);
            break;

        case TEST_SUBCMD_ROTATE_CCW:
            send_mouse_report(0, 5, 0, 0);
            break;

        case TEST_SUBCMD_BTN_PRESS:
            // Send press immediately, queue release after 50ms
            send_mouse_report(1, 0, 0, 0);
            test_pending_event.pending = true;
            test_pending_event.fire_time = make_timeout_time_us(TEST_BTN_RELEASE_DELAY_US);
            test_pending_event.buttons = 0;
            test_pending_event.x = 0;
            test_pending_event.y = 0;
            break;

        case TEST_SUBCMD_NAV_UP:
            send_mouse_report(0, 0, -5, 0);
            test_pending_event.pending = true;
            test_pending_event.fire_time = make_timeout_time_us(TEST_NAV_SECOND_EVENT_US);
            test_pending_event.buttons = 0;
            test_pending_event.x = 0;
            test_pending_event.y = -5;
            break;

        case TEST_SUBCMD_NAV_DOWN:
            send_mouse_report(0, 0, 5, 0);
            test_pending_event.pending = true;
            test_pending_event.fire_time = make_timeout_time_us(TEST_NAV_SECOND_EVENT_US);
            test_pending_event.buttons = 0;
            test_pending_event.x = 0;
            test_pending_event.y = 5;
            break;

        case TEST_SUBCMD_NAV_LEFT:
            send_mouse_report(0, -5, 0, 0);
            test_pending_event.pending = true;
            test_pending_event.fire_time = make_timeout_time_us(TEST_NAV_SECOND_EVENT_US);
            test_pending_event.buttons = 0;
            test_pending_event.x = -5;
            test_pending_event.y = 0;
            break;

        case TEST_SUBCMD_NAV_RIGHT:
            send_mouse_report(0, 5, 0, 0);
            test_pending_event.pending = true;
            test_pending_event.fire_time = make_timeout_time_us(TEST_NAV_SECOND_EVENT_US);
            test_pending_event.buttons = 0;
            test_pending_event.x = 5;
            test_pending_event.y = 0;
            break;

        default:
            break;
    }
}
#endif // ENABLE_TEST_COMMANDS

// Handles a serial command once received
void handle_command() {
    // Check for minimum command length (1 byte for command type at least)
    if (serial_buf_pos < 1) return;

    // Debug info should only be displayed if debug mode is enabled
    if (DEBUG_MODE) {
        char debug_buf[32];
        snprintf(debug_buf, sizeof(debug_buf), "CMD: %02X LEN: %d", serial_buf[0], serial_buf_pos);
        // Save current display content to draw debug info at bottom
        ssd1306_draw_text(0, 56, debug_buf);
    }

    // Process command based on first byte
    switch (serial_buf[0]) {
        case CMD_CLEAR:
            // Simply clear the display without adding any debug text
            ssd1306_clear();
            break;

        case CMD_DRAW_TEXT:
            // Format: CMD_DRAW_TEXT, x, y, len, text...
            if (serial_buf_pos >= 5) {
                uint8_t x = serial_buf[1];
                uint8_t y = serial_buf[2];
                uint8_t text_len = serial_buf[3];

                // Clamp text length to available buffer
                if (text_len > MAX_CMD_SIZE - 4) text_len = MAX_CMD_SIZE - 4;
                if (text_len > serial_buf_pos - 4) text_len = serial_buf_pos - 4;

                // Null-terminate the text
                serial_buf[4 + text_len] = 0;

                // Draw text starting from position 4 in buffer
                ssd1306_draw_text(x, y, (char*)&serial_buf[4]);
            }
            break;

        case CMD_SET_CURSOR:
            // Format: CMD_SET_CURSOR, x, y
            if (serial_buf_pos >= 3) {
                uint8_t x = serial_buf[1];
                uint8_t y = serial_buf[2];
                ssd1306_set_cursor(x, y);
                
                if (DEBUG_MODE) {
                    char debug_buf[32];
                    snprintf(debug_buf, sizeof(debug_buf), "Cursor: %d,%d", x, y);
                    ssd1306_draw_text(0, 48, debug_buf);
                }
            }
            break;

        case CMD_INVERT:
            // Format: CMD_INVERT, value (0 or 1)
            if (serial_buf_pos >= 2) {
                bool invert = serial_buf[1] > 0;
                ssd1306_invert(invert);
                
                if (DEBUG_MODE) {
                    ssd1306_draw_text(0, 48, invert ? "Invert: ON" : "Invert: OFF");
                }
            }
            break;
        case CMD_BRIGHTNESS:
            // Format: CMD_BRIGHTNESS, brightness_level (0-255)
            if (serial_buf_pos >= 2) {
                uint8_t brightness = serial_buf[1];
                ssd1306_set_brightness(brightness);

                if (DEBUG_MODE) {
                    char debug_buf[32];
                    snprintf(debug_buf, sizeof(debug_buf), "Brightness: %d", brightness);
                    ssd1306_draw_text(0, 48, debug_buf);
                }
            }
            break;

        case CMD_PROGRESS_BAR:
            // Format: CMD_PROGRESS_BAR, x, y, width, height, progress (0-100)
            if (serial_buf_pos >= 6) {
                uint8_t x = serial_buf[1];
                uint8_t y = serial_buf[2];
                uint8_t width = serial_buf[3];
                uint8_t height = serial_buf[4];
                uint8_t progress = serial_buf[5];

                ssd1306_draw_progress_bar(x, y, width, height, progress);

                if (DEBUG_MODE) {
                    char debug_buf[32];
                    snprintf(debug_buf, sizeof(debug_buf), "Progress: %d%%", progress);
                    ssd1306_draw_text(0, 48, debug_buf);
                }
            }
            break;

        case CMD_POWER:
            // Format: CMD_POWER, value (0 or 1)
            if (serial_buf_pos >= 2) {
                bool power = serial_buf[1] > 0;
                ssd1306_power(power);

                if (DEBUG_MODE) {
                    ssd1306_draw_text(0, 48, power ? "Power: ON" : "Power: OFF");
                }
            }
            break;

        default:
            // Unknown command
            if (DEBUG_MODE) {
                char debug_buf[32];
                snprintf(debug_buf, sizeof(debug_buf), "Unknown CMD: %02X", serial_buf[0]);
                ssd1306_draw_text(0, 48, debug_buf);
            }
            break;
    }

    // Reset buffer position for next command
    serial_buf_pos = 0;
}

// CDC callback when line state changes
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void) itf;
    (void) rts;

    // When DTR is deasserted, reset the command buffer
    if (!dtr) {
        serial_buf_pos = 0;
        text_cmd_pending = false;
    }
}

// CDC callback when data is received
void tud_cdc_rx_cb(uint8_t itf) {
    (void) itf;

    // Process structured commands
    // Read data until FIFO is empty or buffer is full
    uint8_t cmd_type = 0;
    bool has_cmd_type = false;

    // If we already have data in the buffer, get the command type
    if (serial_buf_pos > 0) {
        cmd_type = serial_buf[0];
        has_cmd_type = true;
    }

    // Read available data
    while (tud_cdc_available() && serial_buf_pos < MAX_CMD_SIZE) {
        uint8_t c;
        tud_cdc_read(&c, 1);

        // Add byte to buffer
        serial_buf[serial_buf_pos++] = c;

        // If this is the first byte, it's our command type
        if (serial_buf_pos == 1) {
            cmd_type = c;
            has_cmd_type = true;
        }
    }

    // If buffer is full but CDC still has data, drain excess to prevent
    // leftover bytes being misinterpreted as the next command header
    if (serial_buf_pos >= MAX_CMD_SIZE) {
        uint8_t discard;
        while (tud_cdc_available()) {
            tud_cdc_read(&discard, 1);
        }
    }

    // If we don't have at least a command type yet, wait for more data
    if (!has_cmd_type) return;

    // Process commands based on type
    switch (cmd_type) {
        case CMD_CLEAR:
            // Clear command only needs one byte
            if (serial_buf_pos >= 1) {
                handle_command();
            }
            break;

        case CMD_DRAW_TEXT:
            // Format: [0x02][x][y][len][text...] — length-based framing
            if (serial_buf_pos >= 4) {
                uint8_t text_len = serial_buf[3];
                if (text_len > MAX_CMD_SIZE - 4) text_len = MAX_CMD_SIZE - 4;
                if (serial_buf_pos >= (uint8_t)(4 + text_len)) {
                    // All text bytes received — process immediately
                    handle_command();
                    text_cmd_pending = false;
                } else if (!text_cmd_pending) {
                    // Still waiting for text bytes — start safety timeout
                    text_cmd_start = get_absolute_time();
                    text_cmd_pending = true;
                }
            }
            break;

        case CMD_SET_CURSOR:
            // Set cursor requires 3 bytes
            if (serial_buf_pos >= 3) {
                handle_command();
            }
            break;

        case CMD_INVERT:
            // Invert requires 2 bytes
            if (serial_buf_pos >= 2) {
                handle_command();
            }
            break;

        case CMD_BRIGHTNESS:
            // Brightness requires 2 bytes
            if (serial_buf_pos >= 2) {
                handle_command();
            }
            break;

        case CMD_PROGRESS_BAR:
            // Progress bar requires 6 bytes
            if (serial_buf_pos >= 6) {
                handle_command();
            }
            break;

        case CMD_POWER:
            // Power requires 2 bytes
            if (serial_buf_pos >= 2) {
                handle_command();
            }
            break;

#ifdef ENABLE_TEST_COMMANDS
        case CMD_TEST:
            // Test command requires 2 bytes: [0xF0][subcommand]
            if (serial_buf_pos >= 2) {
                handle_test_command(serial_buf[1]);
                serial_buf_pos = 0;
            }
            break;
#endif

        default:
            // Unknown command, just reset the buffer
            serial_buf_pos = 0;
            break;
    }

}

// HID callbacks
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;  // Not implemented, just stub to avoid linker error
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;

    // Not implemented, just stub to avoid linker error
}

int main() {
    // Initialize board
    stdio_init_all();

    // Initialize TinyUSB
    tusb_init();

    // Initialize the SSD1306 OLED display
    ssd1306_init();

    // Initialize the rotary encoder
    setup_rotary_encoder();

    // Display startup message - only show briefly
    ssd1306_clear();
    //ssd1306_draw_text(0, 0, "USB HID Display");
    ssd1306_draw_text(0, 0, "Booting.......");
    //ssd1306_draw_text(0, 16, "Ready...");
    
    // Wait a moment to show startup message then clear
    sleep_ms(2000);
    //ssd1306_clear();

    // Main loop
    while (1) {
        // TinyUSB device task
        tud_task();

        // Process rotary encoder
        process_rotary_encoder();

        // Safety fallback: flush CMD_DRAW_TEXT if transfer stalls (length-based
        // framing should complete before this, but protects against incomplete sends)
        if (text_cmd_pending && absolute_time_diff_us(text_cmd_start, get_absolute_time()) >= TEXT_CMD_TIMEOUT_US) {
            // Read any last data that arrived
            while (tud_cdc_available() && serial_buf_pos < MAX_CMD_SIZE) {
                tud_cdc_read(&serial_buf[serial_buf_pos++], 1);
            }
            handle_command();
            text_cmd_pending = false;
        }

#ifdef ENABLE_TEST_COMMANDS
        // Fire pending test event (delayed button release or second nav event)
        if (test_pending_event.pending && absolute_time_diff_us(test_pending_event.fire_time, get_absolute_time()) >= 0) {
            send_mouse_report(test_pending_event.buttons, test_pending_event.x, test_pending_event.y, 0);
            test_pending_event.pending = false;
        }
#endif

        // Handle CDC data processing more aggressively
        uint32_t start = time_us_32();
        
        // Check multiple times for CDC data during each loop cycle
        for (int i = 0; i < 10 && (time_us_32() - start < 1000); i++) {
            // Process CDC data if available
            if (tud_cdc_available()) {
                // Handle incoming data with appropriate callback
                tud_cdc_rx_cb(0);
            }
            
            // Give USB stack time to process data
            tud_task();
            
            // Small yield
            sleep_us(10);
        }
        
        // Small delay to avoid hogging the CPU
        sleep_us(100);
    }

    return 0;
}
