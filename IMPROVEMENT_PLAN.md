# USB HID Display Firmware - Improvement Plan

## Overview
Iterative improvement plan for the RP2040 USB HID Display firmware.
Each item will be implemented, tested, and verified before moving to the next.

---

## Phase 1: Critical Bug Fixes

### 1.1 Buffer overflow on CMD_DRAW_TEXT null-termination
- **File:** `rp2040/src/main.cpp:42`
- **Problem:** `serial_buf[serial_buf_pos] = 0` writes past end of `serial_buf[128]` when `serial_buf_pos == MAX_CMD_SIZE`
- **Fix:** Clamp index to `MAX_CMD_SIZE - 1` before null-terminating
- **Status:** [x] Done

### 1.2 malloc without NULL check in ssd1306_data()
- **File:** `rp2040/src/ssd1306.cpp:50-60`
- **Problem:** `malloc(len + 1)` result is dereferenced without NULL check; causes hard-fault on allocation failure
- **Fix:** Replaced with static buffer `i2c_data_buf[1025]`; removed malloc/free and `<cstdlib>` dependency entirely
- **Status:** [x] Done

### 1.3 CR/LF handler underflow on serial_buf_pos
- **File:** `rp2040/src/main.cpp:243-250`
- **Problem:** After `handle_command()` resets `serial_buf_pos` to 0, CR/LF check could process incomplete commands
- **Fix:** Strip CR/LF before processing; only call `handle_command()` if buffer still has data after strip
- **Status:** [x] Done

### 1.4 Progress bar out-of-bounds writes
- **File:** `rp2040/src/ssd1306.cpp:235-239`
- **Problem:** No validation that `x + width <= 128` or `y + height <= 64`; host-supplied values can cause `display_buffer` overflow
- **Fix:** Clamp width/height to display bounds; early return for out-of-range or degenerate dimensions
- **Status:** [x] Done

---

## Phase 2: Robustness Improvements

### 2.1 I2C calls hang forever on bus errors
- **Files:** `rp2040/src/ssd1306.cpp:47,59`
- **Problem:** `i2c_write_blocking()` will block forever if SSD1306 is disconnected or I2C bus is stuck
- **Fix:** Replaced with `i2c_write_timeout_us()` with 10ms timeout
- **Status:** [x] Done

### 2.2 Blocking sleep inside USB CDC callback
- **File:** `rp2040/src/main.cpp:193-200,305-313`
- **Problem:** `sleep_ms(5)` inside `tud_cdc_rx_cb()` blocks USB stack processing
- **Fix:** Non-blocking accumulation window using `text_cmd_pending` flag + timestamp; flushed in main loop after 5ms
- **Status:** [x] Done

### 2.3 Shared debounce timer for all direction buttons
- **File:** `rp2040/src/rotary_encoder.cpp:25-42`
- **Problem:** All 4 direction buttons shared `last_dir_btn_time`; pressing one suppresses others
- **Fix:** Each `dir_button_t` struct has its own `last_debounce_time`
- **Status:** [x] Done

### 2.4 No tud_hid_ready() check before sending reports
- **File:** `rp2040/src/rotary_encoder.cpp:111`
- **Problem:** `tud_hid_report()` called without checking if HID interface is ready
- **Fix:** Added `if (!tud_hid_ready()) return;` guard at top of `send_mouse_report()`
- **Status:** [x] Done

---

## Phase 3: Portability & Correctness

### 3.1 Signed char comparison in ssd1306_draw_char()
- **File:** `rp2040/src/ssd1306.cpp:136`
- **Problem:** `char` is unsigned on ARM by default, so `c < 0` is always false
- **Fix:** Changed to `if ((uint8_t)c > 127) c = '?';`
- **Status:** [x] Done

### 3.2 Duplicate #define between main.h and main.cpp
- **Files:** `rp2040/src/main.h`, `rp2040/src/main.cpp`
- **Problem:** Command codes defined in both files; main.h was missing newer commands
- **Fix:** All 7 command definitions consolidated in main.h; duplicates removed from main.cpp
- **Status:** [x] Done

---

## Phase 4: Features & Enhancements

### 4.1 Hardcoded serial number - use chip unique ID
- **File:** `rp2040/src/usb_descriptors.c:97-107`
- **Problem:** Serial number was hardcoded "123456"; multiple devices conflict
- **Fix:** Added `get_usb_serial()` using `pico_get_unique_board_id_string()`; lazy-initialized on first USB descriptor request
- **Status:** [x] Done

---

## Phase 5: Code Quality & Cleanup

### 5.1 Inconsistent indentation in main.cpp
- **File:** `rp2040/src/main.cpp`
- **Problem:** CMD_BRIGHTNESS and CMD_PROGRESS_BAR cases used mixed tabs/spaces
- **Fix:** Normalized to consistent 4-space indentation throughout entire file
- **Status:** [x] Done

### 5.2 Direction button code repetition
- **File:** `rp2040/src/rotary_encoder.cpp:25-42,207-234`
- **Problem:** Same logic repeated 4 times for left/right/top/bot (~80 lines)
- **Fix:** Refactored into `dir_button_t` struct array + single loop (~25 lines)
- **Status:** [x] Done

### 5.3 Dead code: direct text mode and cdc_buffer
- **File:** `rp2040/src/main.cpp`
- **Problem:** `command_mode` always true; `toggle_command_mode()` never called; direct text mode unreachable
- **Fix:** Removed `command_mode`, `cdc_buffer`, `toggle_command_mode()`, and direct text mode branch
- **Status:** [x] Done

### 5.4 PIO-based rotary encoder (optional enhancement)
- **File:** `rp2040/src/rotary_encoder.cpp`
- **Problem:** CPU-polled quadrature decoding can miss steps under load and wastes CPU cycles
- **Fix:** Implement PIO state machine for jitter-free, zero-CPU quadrature decoding
- **Status:** [ ] Pending (optional - discuss before implementing)

---

## Phase 6: Test Commands for Automated Hardware Testing

### 6.0 Add build-time test command support (0xF0)
- **Files:** `rp2040/CMakeLists.txt`, `rp2040/src/main.h`, `rp2040/src/main.cpp`, `rp2040/src/rotary_encoder.cpp`
- **Problem:** No way to inject HID input events programmatically for automated end-to-end testing
- **Fix:** Added `CMD_TEST` (0xF0) with 8 subcommands (ping, rotate CW/CCW, button press, nav up/down/left/right) behind `-DENABLE_TEST_COMMANDS=ON` build flag. Uses existing `send_mouse_report()` for byte-identical HID output. Delayed events (button release, second nav event) processed via pending event queue in main loop.
- **Status:** [x] Done

---

## Phase 7: Coordinated Protocol Improvements (requires micropanel update)

These changes require synchronized updates to both the RP2040 firmware and the micropanel daemon.

### 7.1 Add length-based framing to CMD_DRAW_TEXT
- **Files:** `rp2040/src/main.cpp`
- **Problem:** `CMD_DRAW_TEXT` (0x02) had no explicit length or terminator. The firmware used a 5ms timeout to decide when the text payload was complete — fragile under USB packet timing variation.
- **Old format:** `[0x02][x][y][text...]`
- **New format:** `[0x02][x][y][len][text...]`
- **Fix:** CDC callback now checks `serial_buf[3]` as length byte and calls `handle_command()` when `serial_buf_pos >= 4 + len`. `handle_command()` reads text from `serial_buf[4]` onwards. Safety timeout retained as fallback for stalled transfers. Pre-flush of pending text removed (no longer needed with deterministic framing).
- **Status:** [x] Done

### 7.2 Remove CR/LF fallback from binary parser
- **Files:** `rp2040/src/main.cpp`
- **Problem:** The firmware treated `\r` (0x0D) and `\n` (0x0A) as command terminators. In a binary protocol, these byte values are legitimate parameter values (e.g., x=10 is 0x0A), causing premature command finalization.
- **Fix:** Removed the CR/LF detection block. All commands now have deterministic framing: fixed-length (0x01, 0x03-0x07) or length-prefixed (0x02).
- **Status:** [x] Done

---

## Phase 8: Runtime Orientation Detection

### 8.1 GPIO jumper for landscape/portrait selection
- **Files:** `main.h`, `main.cpp`, `ssd1306.cpp`, `rotary_encoder.cpp`, `usb_descriptors.c`, `CMakeLists.txt`
- **Problem:** Required two separate firmware builds (`-DDISPLAY_ORIENTATION=portrait` vs landscape)
- **Fix:** Added runtime orientation detection via GPIO 29 jumper (GND = portrait, floating = landscape). Replaced all 7 compile-time `#ifdef DISPLAY_PORTRAIT` blocks with runtime `if (g_portrait)` checks. Removed `DISPLAY_ORIENTATION` CMake option. USB product string now dynamically reflects detected orientation.
- **Status:** [x] Done

### 8.2 Progress bar read-modify-write for shared display pages
- **File:** `rp2040/src/ssd1306.cpp`
- **Problem:** `ssd1306_draw_progress_bar()` used destructive page writes (`display_buffer[pos] = mask`), overwriting text on shared 8-pixel pages (common in portrait mode after Y-flip)
- **Fix:** Compute `bar_mask` per page (bits belonging to bar's Y range), then read-modify-write: `(display_buffer[pos] & ~bar_mask) | (mask & bar_mask)` to preserve other content
- **Status:** [x] Done

---

## Summary

- **19 of 20 items completed** (items 1.1 through 5.3, 6.0, 7.1, 7.2, 8.1, 8.2)
- **1 item pending** (5.4 PIO encoder - optional, requires discussion)
- **Files modified:** main.h, main.cpp, ssd1306.cpp, rotary_encoder.cpp, usb_descriptors.c, CMakeLists.txt
- **Next step:** Test runtime orientation with GPIO 29 jumper on hardware
