#include "main.h"
#include "font8x8_basic.h" // This will be created later
#include <cstdlib> // For malloc and free

// SSD1306 OLED display is 128x64 pixels
#define SSD1306_WIDTH           128
#define SSD1306_HEIGHT          64
#define SSD1306_PAGE_HEIGHT     8 // 8 pixels per page

// SSD1306 commands
#define SSD1306_SET_CONTRAST             0x81
#define SSD1306_DISPLAY_RAM              0xA4
#define SSD1306_DISPLAY_ALLON            0xA5
#define SSD1306_DISPLAY_NORMAL           0xA6
#define SSD1306_DISPLAY_INVERTED         0xA7
#define SSD1306_DISPLAY_OFF              0xAE
#define SSD1306_DISPLAY_ON               0xAF
#define SSD1306_SET_DISPLAY_OFFSET       0xD3
#define SSD1306_SET_COM_PINS             0xDA
#define SSD1306_SET_VCOM_DETECT          0xDB
#define SSD1306_SET_DISPLAY_CLOCK_DIV    0xD5
#define SSD1306_SET_PRECHARGE            0xD9
#define SSD1306_SET_MULTIPLEX            0xA8
#define SSD1306_SET_LOW_COLUMN           0x00
#define SSD1306_SET_HIGH_COLUMN          0x10
#define SSD1306_SET_START_LINE           0x40
#define SSD1306_MEMORY_MODE              0x20
#define SSD1306_COLUMN_ADDR              0x21
#define SSD1306_PAGE_ADDR                0x22
#define SSD1306_COM_SCAN_INC             0xC0
#define SSD1306_COM_SCAN_DEC             0xC8
#define SSD1306_SEG_REMAP                0xA0
#define SSD1306_SEG_REMAP_REVERSE        0xA1  // Column address 127 mapped to SEG0
#define SSD1306_CHARGE_PUMP              0x8D

// Display buffer
static uint8_t display_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8] = {0};

static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

// Function to send command to SSD1306
static void ssd1306_command(uint8_t command) {
    uint8_t buf[2] = {0x00, command}; // Control byte (0x00) followed by command
    i2c_write_blocking(I2C_PORT, SSD1306_ADDR, buf, 2, false);
}

// Function to send data to SSD1306
static void ssd1306_data(uint8_t* data, size_t len) {
    uint8_t *buf = (uint8_t*)malloc(len + 1);
    buf[0] = 0x40; // Control byte (0x40) for data
    memcpy(buf + 1, data, len);
    i2c_write_blocking(I2C_PORT, SSD1306_ADDR, buf, len + 1, false);
    free(buf);
}

// Initialize SSD1306 OLED display
void ssd1306_init() {
    // Initialize I2C port
    i2c_init(I2C_PORT, 400000); // 400 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Give display time to power up
    sleep_ms(100);

    // Initialize display
    ssd1306_command(SSD1306_DISPLAY_OFF);
    ssd1306_command(SSD1306_SET_DISPLAY_CLOCK_DIV);
    ssd1306_command(0x80); // Suggested ratio
    ssd1306_command(SSD1306_SET_MULTIPLEX);
    ssd1306_command(SSD1306_HEIGHT - 1);
    ssd1306_command(SSD1306_SET_DISPLAY_OFFSET);
    ssd1306_command(0x00);
    ssd1306_command(SSD1306_SET_START_LINE | 0x00);
    ssd1306_command(SSD1306_CHARGE_PUMP);
    ssd1306_command(0x14); // Enable charge pump
    ssd1306_command(SSD1306_MEMORY_MODE);
    ssd1306_command(0x00); // Horizontal addressing mode

    // Set orientation for normal display (top-left origin)
    ssd1306_command(SSD1306_SEG_REMAP_REVERSE); // 0xA1 
    ssd1306_command(SSD1306_COM_SCAN_DEC);      // 0xC8

    ssd1306_command(SSD1306_SET_COM_PINS);
    ssd1306_command(0x12);
    ssd1306_command(SSD1306_SET_CONTRAST);
    ssd1306_command(0xCF);
    ssd1306_command(SSD1306_SET_PRECHARGE);
    ssd1306_command(0xF1);
    ssd1306_command(SSD1306_SET_VCOM_DETECT);
    ssd1306_command(0x40);
    ssd1306_command(SSD1306_DISPLAY_RAM);
    ssd1306_command(SSD1306_DISPLAY_NORMAL);
    ssd1306_command(SSD1306_DISPLAY_ON);

    // Clear the display
    ssd1306_clear();
}

// Clear the display
void ssd1306_clear() {
    memset(display_buffer, 0, sizeof(display_buffer));

    // Set address range for whole display
    ssd1306_command(SSD1306_PAGE_ADDR);
    ssd1306_command(0);
    ssd1306_command(SSD1306_HEIGHT / 8 - 1);
    ssd1306_command(SSD1306_COLUMN_ADDR);
    ssd1306_command(0);
    ssd1306_command(SSD1306_WIDTH - 1);

    // Send cleared buffer to display
    ssd1306_data(display_buffer, sizeof(display_buffer));

    // Reset cursor position
    cursor_x = 0;
    cursor_y = 0;
}

// Set cursor position
void ssd1306_set_cursor(uint8_t x, uint8_t y) {
    cursor_x = x;
    cursor_y = y;
}

// Draw a character at current cursor position
static void ssd1306_draw_char(char c) {
    if (c < 0 || c > 127) c = '?'; // Handle non-ASCII chars

    // Calculate buffer position
    int page = cursor_y / 8;  // Page = y / 8 
    int col = cursor_x;       // Column = x

    // Check bounds
    if (page >= SSD1306_HEIGHT / 8 || col >= SSD1306_WIDTH - 8) return;

    // The SSD1306 display writes bytes vertically (each byte is 8 vertical pixels)
    // We need to transpose the character data (convert rows to columns)
    uint8_t transposed[8] = {0};
    
    // Transpose the character (swap rows and columns)
    for (int srcRow = 0; srcRow < 8; srcRow++) {
        uint8_t src_byte = font8x8_basic[(uint8_t)c][srcRow];
        
        for (int srcCol = 0; srcCol < 8; srcCol++) {
            if (src_byte & (1 << srcCol)) {
                // Set the corresponding bit in the transposed character
                // Source bit at (srcRow, srcCol) goes to (srcCol, srcRow)
                transposed[srcCol] |= (1 << srcRow);
            }
        }
    }
    
    // Copy transposed character to buffer
    for (int i = 0; i < 8; i++) {
        if (col + i < SSD1306_WIDTH) {
            // Calculate position in buffer (page * width + column + i)
            int pos = page * SSD1306_WIDTH + (col + i);
            display_buffer[pos] = transposed[i];
        }
    }

    // Update the display for this character
    ssd1306_command(SSD1306_PAGE_ADDR);
    ssd1306_command(page);
    ssd1306_command(page);
    ssd1306_command(SSD1306_COLUMN_ADDR);
    ssd1306_command(col);
    ssd1306_command(col + 7);

    // Send the character data to display
    ssd1306_data(&display_buffer[page * SSD1306_WIDTH + col], 8);
    
    // Advance cursor - move 8 pixels to the right
    cursor_x += 8;
    
    // Wrap to next line if needed
    if (cursor_x > SSD1306_WIDTH - 8) {
        cursor_x = 0;
        cursor_y += 8; // Move down one character row (8 pixels)
        if (cursor_y >= SSD1306_HEIGHT) {
            cursor_y = 0; // Wrap to top if we reach the bottom
        }
    }
}

// Draw text at specified or current cursor position
void ssd1306_draw_text(uint8_t x, uint8_t y, const char* text) {
    ssd1306_set_cursor(x, y);
    while (*text) {
        ssd1306_draw_char(*text++);
    }
}

// Invert display
void ssd1306_invert(bool invert) {
    if (invert) {
        ssd1306_command(SSD1306_DISPLAY_INVERTED);
    } else {
        ssd1306_command(SSD1306_DISPLAY_NORMAL);
    }
}

// Power on/off display
void ssd1306_power(bool power) {
    if (power) {
        ssd1306_command(SSD1306_DISPLAY_ON);
    } else {
        ssd1306_command(SSD1306_DISPLAY_OFF);
    }
}
// Set display brightness/contrast (0-255)
void ssd1306_set_brightness(uint8_t brightness) {
    ssd1306_command(SSD1306_SET_CONTRAST);
    ssd1306_command(brightness);
}

// Draw a progress bar
// x, y: top-left coordinates of the progress bar
// width: total width of the progress bar in pixels
// height: height of the progress bar in pixels
// progress: value between 0 and 100
void ssd1306_draw_progress_bar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t progress) {
    // Ensure progress is within range
    if (progress > 100) progress = 100;

    // Calculate progress width in pixels
    uint8_t progress_width = (width * progress) / 100;

    // Calculate which pages the bar spans
    uint8_t start_page = y / 8;
    uint8_t end_page = (y + height - 1) / 8;

    // Draw the progress bar outline
    for (uint8_t page = start_page; page <= end_page; page++) {
        for (uint8_t col = x; col < x + width; col++) {
            uint8_t mask = 0;

            // Calculate which bits in the byte should be set
            for (uint8_t bit = 0; bit < 8; bit++) {
                uint8_t pixel_y = page * 8 + bit;
                if (pixel_y >= y && pixel_y < y + height) {
                    // This bit is part of the progress bar
                    if (col == x || col == x + width - 1 ||
                        pixel_y == y || pixel_y == y + height - 1) {
                        // This pixel is part of the border
                        mask |= (1 << bit);
                    }
                    else if (col < x + progress_width) {
                        // This pixel is part of the filled area
                        mask |= (1 << bit);
                    }
                }
            }

            // Update display buffer
            int pos = page * SSD1306_WIDTH + col;
            display_buffer[pos] = mask;
        }
    }

    // Update the display for the affected area
    for (uint8_t page = start_page; page <= end_page; page++) {
        ssd1306_command(SSD1306_PAGE_ADDR);
        ssd1306_command(page);
        ssd1306_command(page);
        ssd1306_command(SSD1306_COLUMN_ADDR);
        ssd1306_command(x);
        ssd1306_command(x + width - 1);

        // Send the progress bar data to display
        ssd1306_data(&display_buffer[page * SSD1306_WIDTH + x], width);
    }
}
