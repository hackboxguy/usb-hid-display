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
            // Format: CMD_DRAW_TEXT, x, y, text...
            if (serial_buf_pos >= 4) {
                uint8_t x = serial_buf[1];
                uint8_t y = serial_buf[2];

                // Null-terminate the text (clamp to avoid overflow)
                if (serial_buf_pos >= MAX_CMD_SIZE) serial_buf_pos = MAX_CMD_SIZE - 1;
                serial_buf[serial_buf_pos] = 0;

                // For CMD_DRAW_TEXT, first clear the screen to avoid 
                // leftover text from previous commands
                //ssd1306_clear();
                
                // Draw text starting from position 3 in buffer
                ssd1306_draw_text(x, y, (char*)&serial_buf[3]);
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

    // If a text command is still accumulating, flush it before reading new data
    // so back-to-back CMD_DRAW_TEXT commands don't merge into one
    if (text_cmd_pending) {
        handle_command();
        text_cmd_pending = false;
    }

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
            // For text command, start a non-blocking accumulation window
            // so we capture the entire text string without blocking USB stack
            if (serial_buf_pos >= 4 && !text_cmd_pending) {
                text_cmd_start = get_absolute_time();
                text_cmd_pending = true;
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

        default:
            // Unknown command, just reset the buffer
            serial_buf_pos = 0;
            break;
    }

    // If command wasn't processed yet (incomplete), check for CR/LF as
    // alternative terminator — but only when buffer still has data
    if (serial_buf_pos > 0 && (serial_buf[serial_buf_pos-1] == '\r' || serial_buf[serial_buf_pos-1] == '\n')) {
        serial_buf_pos--; // strip the CR/LF before processing
        if (serial_buf_pos > 0) {
            handle_command();
        }
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

        // Flush pending CMD_DRAW_TEXT after accumulation window expires
        if (text_cmd_pending && absolute_time_diff_us(text_cmd_start, get_absolute_time()) >= TEXT_CMD_TIMEOUT_US) {
            // Read any last data that arrived during the window
            while (tud_cdc_available() && serial_buf_pos < MAX_CMD_SIZE) {
                tud_cdc_read(&serial_buf[serial_buf_pos++], 1);
            }
            handle_command();
            text_cmd_pending = false;
        }

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
