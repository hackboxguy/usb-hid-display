#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/unique_id.h"
#include "tusb.h"

// I2C defines
#define I2C_PORT        i2c0
#define I2C_SDA_PIN     4
#define I2C_SCL_PIN     5

// SSD1306 defines
#define SSD1306_ADDR    0x3C

// Serial protocol commands
#define CMD_CLEAR       0x01
#define CMD_DRAW_TEXT   0x02
#define CMD_SET_CURSOR  0x03
#define CMD_INVERT      0x04

// Buffer sizes
#define MAX_CMD_SIZE    128

// Function declarations only (no implementations)
void ssd1306_init();
void ssd1306_clear();
void ssd1306_draw_text(uint8_t x, uint8_t y, const char* text);
void ssd1306_invert(bool invert);
void ssd1306_power(bool power);
void ssd1306_set_cursor(uint8_t x, uint8_t y);
void ssd1306_set_brightness(uint8_t brightness);
void ssd1306_draw_progress_bar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t progress);

// Rotary encoder functions
void setup_rotary_encoder();
void process_rotary_encoder();

#endif // MAIN_H
