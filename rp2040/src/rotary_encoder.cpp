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
static bool button_state = false; // Current button state (pressed or not)
static bool button_changed = false; // Flag to indicate button state changed
static absolute_time_t last_button_time = {0}; // For debouncing
static absolute_time_t last_rotation_time = {0}; // For rotation debouncing
static const uint32_t DEBOUNCE_TIME_US = 5000; // 5ms debounce (increased)
static bool last_report_state = false; // Track last reported button state

// Direction button state tracking
static bool left_btn_last_state = false;
static bool right_btn_last_state = false;
static bool top_btn_last_state = false;
static bool bot_btn_last_state = false;
static absolute_time_t last_dir_btn_time = {0}; // Separate for direction buttons

// Flag to track second event needs for direction buttons
static bool left_btn_second_pending = false;
static bool right_btn_second_pending = false;
static bool top_btn_second_pending = false;
static bool bot_btn_second_pending = false;
static absolute_time_t left_btn_first_time = {0};
static absolute_time_t right_btn_first_time = {0};
static absolute_time_t top_btn_first_time = {0};
static absolute_time_t bot_btn_first_time = {0};
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
    absolute_time_t now = get_absolute_time();

    // Debounce protection
    if (absolute_time_diff_us(last_button_time, now) < DEBOUNCE_TIME_US) {
        return;
    }

    last_button_time = now;

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
    gpio_init(LEFT_BTN_PIN);
    gpio_init(RIGHT_BTN_PIN);
    gpio_init(TOP_BTN_PIN);
    gpio_init(BOT_BTN_PIN);

    gpio_set_dir(LEFT_BTN_PIN, GPIO_IN);
    gpio_set_dir(RIGHT_BTN_PIN, GPIO_IN);
    gpio_set_dir(TOP_BTN_PIN, GPIO_IN);
    gpio_set_dir(BOT_BTN_PIN, GPIO_IN);

    gpio_pull_up(LEFT_BTN_PIN);
    gpio_pull_up(RIGHT_BTN_PIN);
    gpio_pull_up(TOP_BTN_PIN);
    gpio_pull_up(BOT_BTN_PIN);

    // Initialize button states
    last_clk_state = gpio_get(ROTARY_CLK_PIN);
    last_dt_state = gpio_get(ROTARY_DT_PIN);
    button_state = !gpio_get(ROTARY_SW_PIN); // Inverted due to pull-up
    last_report_state = button_state; // Initialize to match

    // Initialize direction button states
    left_btn_last_state = !gpio_get(LEFT_BTN_PIN);
    right_btn_last_state = !gpio_get(RIGHT_BTN_PIN);
    top_btn_last_state = !gpio_get(TOP_BTN_PIN);
    bot_btn_last_state = !gpio_get(BOT_BTN_PIN);
}

// Send a mouse report
static void send_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
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
    if (absolute_time_diff_us(last_button_time, now) > 50000) { // 50ms
        // Read button state directly as a backup to interrupts
        bool new_state = !gpio_get(ROTARY_SW_PIN);
        if (new_state != button_state) {
            button_state = new_state;
            button_changed = true;
            last_button_time = now;
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

    // Process any pending second events for direction buttons
    if (left_btn_second_pending) {
        if (absolute_time_diff_us(left_btn_first_time, now) >= SECOND_EVENT_DELAY_US) {
            // Send second event for left button
            send_mouse_report(button_state ? 1 : 0, -5, 0, 0);
            left_btn_second_pending = false;
        }
    }
    if (right_btn_second_pending) {
        if (absolute_time_diff_us(right_btn_first_time, now) >= SECOND_EVENT_DELAY_US) {
            // Send second event for right button
            send_mouse_report(button_state ? 1 : 0, 5, 0, 0);
            right_btn_second_pending = false;
        }
    }
    if (top_btn_second_pending) {
        if (absolute_time_diff_us(top_btn_first_time, now) >= SECOND_EVENT_DELAY_US) {
            // Send second event for top button
            send_mouse_report(button_state ? 1 : 0, 0, -5, 0);
            top_btn_second_pending = false;
        }
    }
    if (bot_btn_second_pending) {
        if (absolute_time_diff_us(bot_btn_first_time, now) >= SECOND_EVENT_DELAY_US) {
            // Send second event for bottom button
            send_mouse_report(button_state ? 1 : 0, 0, 5, 0);
            bot_btn_second_pending = false;
        }
    }

    // Process the direction buttons
    // Only check buttons after sufficient time has passed (to avoid bouncing)
    if (absolute_time_diff_us(last_dir_btn_time, now) > DEBOUNCE_TIME_US) {
        // Check each direction button (active low with pull-up)
        bool left_btn_current = !gpio_get(LEFT_BTN_PIN);
        bool right_btn_current = !gpio_get(RIGHT_BTN_PIN);
        bool top_btn_current = !gpio_get(TOP_BTN_PIN);
        bool bot_btn_current = !gpio_get(BOT_BTN_PIN);

        // Left button - send REL_X value -5
        if (left_btn_current != left_btn_last_state) {
            if (left_btn_current) {
                // Button press - send first event and schedule second
                send_mouse_report(button_state ? 1 : 0, -5, 0, 0);
                left_btn_first_time = now;
                left_btn_second_pending = true;
            }
            left_btn_last_state = left_btn_current;
            last_dir_btn_time = now;
        }

        // Right button - send REL_X value 5
        if (right_btn_current != right_btn_last_state) {
            if (right_btn_current) {
                // Button press - send first event and schedule second
                send_mouse_report(button_state ? 1 : 0, 5, 0, 0);
                right_btn_first_time = now;
                right_btn_second_pending = true;
            }
            right_btn_last_state = right_btn_current;
            last_dir_btn_time = now;
        }

        // Top button - send REL_Y value -5
        if (top_btn_current != top_btn_last_state) {
            if (top_btn_current) {
                // Button press - send first event and schedule second
                send_mouse_report(button_state ? 1 : 0, 0, -5, 0);
                top_btn_first_time = now;
                top_btn_second_pending = true;
            }
            top_btn_last_state = top_btn_current;
            last_dir_btn_time = now;
        }

        // Bottom button - send REL_Y value 5
        if (bot_btn_current != bot_btn_last_state) {
            if (bot_btn_current) {
                // Button press - send first event and schedule second
                send_mouse_report(button_state ? 1 : 0, 0, 5, 0);
                bot_btn_first_time = now;
                bot_btn_second_pending = true;
            }
            bot_btn_last_state = bot_btn_current;
            last_dir_btn_time = now;
        }
    }
}
