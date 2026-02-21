#include "main.h"

// Rotary encoder GPIO pins
#define ROTARY_CLK_PIN  27 //10 //REL_X
#define ROTARY_DT_PIN   26 //11
//#define ROTARY_SW_PIN 14 //ENTER_BTN: MOUSE_BTN_LEFT

//gpio config for up/down/left/right directional buttons
#define LEFT_BTN_PIN    7  // REL_X value -5
#define RIGHT_BTN_PIN   6  // REL_X value 5
#define TOP_BTN_PIN     15 // REL_Y value 5(mapped to down button)
#define BOT_BTN_PIN     8  // REL_Y value -5(mapped to up button)
#define ROTARY_SW_PIN   14 //ENTER_BTN: MOUSE_BTN_LEFT

// Global state variables
static int last_clk_state = 0;
static int last_dt_state = 0;
static volatile bool button_state = false; // Written in IRQ, read in main loop
static volatile bool button_changed = false; // Written in IRQ, read in main loop
static volatile uint32_t last_button_time_us = 0; // For debouncing (atomic 32-bit, IRQ-safe)
static absolute_time_t last_rotation_time = {0}; // For rotation debouncing
static const uint32_t DEBOUNCE_TIME_US = 5000; // 5ms debounce (increased)
static bool last_report_state = false; // Track last reported button state

// Direction button configuration and state
typedef struct {
    uint gpio_pin;
    int8_t rel_x;
    int8_t rel_y;
    bool last_state;
    absolute_time_t last_debounce_time;
    bool second_pending;
    absolute_time_t first_event_time;
} dir_button_t;

#define NUM_DIR_BUTTONS 4
#ifdef DISPLAY_PORTRAIT
// Portrait: buttons rotated 90° CCW (LEFT→UP, RIGHT→DOWN, UP→RIGHT, DOWN→LEFT)
static dir_button_t dir_buttons[NUM_DIR_BUTTONS] = {
    { LEFT_BTN_PIN,    0, -5, false, {0}, false, {0} },  // LEFT btn → UP
    { RIGHT_BTN_PIN,   0,  5, false, {0}, false, {0} },  // RIGHT btn → DOWN
    { TOP_BTN_PIN,     5,  0, false, {0}, false, {0} },  // UP btn → RIGHT
    { BOT_BTN_PIN,    -5,  0, false, {0}, false, {0} },  // DOWN btn → LEFT
};
#else
// Landscape (default)
static dir_button_t dir_buttons[NUM_DIR_BUTTONS] = {
    { LEFT_BTN_PIN,   -5,  0, false, {0}, false, {0} },  // Left
    { RIGHT_BTN_PIN,   5,  0, false, {0}, false, {0} },  // Right
    { TOP_BTN_PIN,     0, -5, false, {0}, false, {0} },  // Top
    { BOT_BTN_PIN,     0,  5, false, {0}, false, {0} },  // Bottom
};
#endif
static const uint32_t SECOND_EVENT_DELAY_US = 16000; // 16ms between events to match rotary

// Mouse report structure
typedef struct {
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} mouse_report_t;

// Callback for button pin interrupt
static void button_callback(uint gpio, uint32_t events) {
    uint32_t now_us = time_us_32();

    // Debounce protection
    if ((now_us - last_button_time_us) < DEBOUNCE_TIME_US) {
        return;
    }

    last_button_time_us = now_us;

    if (gpio == ROTARY_SW_PIN) {
        // Update button state based on the pin state (inverted due to pull-up)
        bool new_state = !gpio_get(ROTARY_SW_PIN);

        // Only process if state actually changed
        if (new_state != button_state) {
            button_state = new_state;
            button_changed = true;
        }
    }
}

// Initialize rotary encoder and button GPIO
void setup_rotary_encoder() {
    // Initialize CLK and DT pins with pull-ups
    gpio_init(ROTARY_CLK_PIN);
    gpio_init(ROTARY_DT_PIN);
    gpio_set_dir(ROTARY_CLK_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_DT_PIN, GPIO_IN);
    gpio_pull_up(ROTARY_CLK_PIN);
    gpio_pull_up(ROTARY_DT_PIN);

    // Initialize button pin with pull-up and interrupts
    gpio_init(ROTARY_SW_PIN);
    gpio_set_dir(ROTARY_SW_PIN, GPIO_IN);
    gpio_pull_up(ROTARY_SW_PIN);
    gpio_set_irq_enabled_with_callback(ROTARY_SW_PIN,
                                      GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
                                      true, &button_callback);

    // Initialize direction buttons with pull-up (since they're active low)
    for (int i = 0; i < NUM_DIR_BUTTONS; i++) {
        gpio_init(dir_buttons[i].gpio_pin);
        gpio_set_dir(dir_buttons[i].gpio_pin, GPIO_IN);
        gpio_pull_up(dir_buttons[i].gpio_pin);
        dir_buttons[i].last_state = !gpio_get(dir_buttons[i].gpio_pin);
    }

    // Initialize rotary encoder states
    last_clk_state = gpio_get(ROTARY_CLK_PIN);
    last_dt_state = gpio_get(ROTARY_DT_PIN);
    button_state = !gpio_get(ROTARY_SW_PIN); // Inverted due to pull-up
    last_report_state = button_state; // Initialize to match
}

// Send a mouse report (non-static: also called by test commands)
void send_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
    if (!tud_hid_ready()) return;

    mouse_report_t report = {
        .buttons = buttons,
        .x = x,
        .y = y,
        .wheel = wheel
    };

    // Send report
    tud_hid_report(0, &report, sizeof(report));

    // Ensure the packet is sent
    tud_task();

    // Small delay to ensure report is processed
    sleep_us(500);
}

// Process the rotary encoder state and send HID reports if needed
void process_rotary_encoder() {
    absolute_time_t now = get_absolute_time();

    // Check if we need to read the button state directly
    uint32_t now_us = time_us_32();
    if ((now_us - last_button_time_us) > 50000) { // 50ms
        // Read button state directly as a backup to interrupts
        bool new_state = !gpio_get(ROTARY_SW_PIN);
        if (new_state != button_state) {
            button_state = new_state;
            button_changed = true;
            last_button_time_us = now_us;
        }
    }

    // Handle button state changes
    if (button_changed || (button_state != last_report_state)) {
        // Send mouse report that matches the current button state
        send_mouse_report(button_state ? 1 : 0, 0, 0, 0);

        // Update state tracking
        button_changed = false;
        last_report_state = button_state;
    }

    // Handle encoder rotation
    int clk_state = gpio_get(ROTARY_CLK_PIN);
    int dt_state = gpio_get(ROTARY_DT_PIN);

    // Detect state change - check both pins for any change
    if (clk_state != last_clk_state || dt_state != last_dt_state) {
        // Debounce protection
        if (absolute_time_diff_us(last_rotation_time, now) < DEBOUNCE_TIME_US) {
            last_clk_state = clk_state;
            last_dt_state = dt_state;
            return;
        }

        last_rotation_time = now;

        // Use Gray code sequence to determine direction
        // Create a 2-bit state value from both pins
        int current_state = (clk_state << 1) | dt_state;
        int last_state = (last_clk_state << 1) | last_dt_state;

        // Detect rotation direction using Gray code transition
        int direction = 0;

        // Detect clockwise motion
        if ((last_state == 0b00 && current_state == 0b01) ||
            (last_state == 0b01 && current_state == 0b11) ||
            (last_state == 0b11 && current_state == 0b10) ||
            (last_state == 0b10 && current_state == 0b00)) {
            direction = 1; // Clockwise
        }
        // Detect counter-clockwise motion
        else if ((last_state == 0b00 && current_state == 0b10) ||
                (last_state == 0b10 && current_state == 0b11) ||
                (last_state == 0b11 && current_state == 0b01) ||
                (last_state == 0b01 && current_state == 0b00)) {
            direction = -1; // Counter-clockwise
        }

        // Send movement based on detected direction
        if (direction == 1) {
            // Clockwise - move mouse right
            send_mouse_report(button_state ? 1 : 0, -5, 0, 0);
        } else if (direction == -1) {
            // Counter-clockwise - move mouse left
            send_mouse_report(button_state ? 1 : 0, 5, 0, 0);
        }

        // Update the last states
        last_clk_state = clk_state;
        last_dt_state = dt_state;
    }

    // Process direction buttons: pending second events and new presses
    uint8_t btn_bits = button_state ? 1 : 0;

    for (int i = 0; i < NUM_DIR_BUTTONS; i++) {
        dir_button_t *btn = &dir_buttons[i];

        // Send second event if pending and delay has elapsed
        if (btn->second_pending) {
            if (absolute_time_diff_us(btn->first_event_time, now) >= SECOND_EVENT_DELAY_US) {
                send_mouse_report(btn_bits, btn->rel_x, btn->rel_y, 0);
                btn->second_pending = false;
            }
        }

        // Poll button with per-button debounce
        if (absolute_time_diff_us(btn->last_debounce_time, now) > DEBOUNCE_TIME_US) {
            bool current = !gpio_get(btn->gpio_pin);
            if (current != btn->last_state) {
                if (current) {
                    send_mouse_report(btn_bits, btn->rel_x, btn->rel_y, 0);
                    btn->first_event_time = now;
                    btn->second_pending = true;
                }
                btn->last_state = current;
                btn->last_debounce_time = now;
            }
        }
    }
}
