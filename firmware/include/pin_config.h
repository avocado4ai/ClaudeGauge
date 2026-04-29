#pragma once

// ============================================================
// Board-specific pin configuration
// ============================================================

#if defined(BOARD_TDISPLAY_S3)

    // Buttons (active LOW, internal pull-up)
    #define BTN_LEFT    0     // GPIO 0  = BOOT button
    #define BTN_RIGHT   14    // GPIO 14 = second button
    #define HAS_TWO_BUTTONS   1

    // Backlight PWM
    #define PIN_BL      38
    #define BL_CHANNEL  0
    #define BL_FREQ     5000
    #define BL_RES      8     // 8-bit resolution (0-255)

    // Power control (T-Display-S3 specific)
    #define PIN_POWER_ON 15
    #define HAS_POWER_PIN 1

    // Capacitive touch (CST816S via I2C)
    #define HAS_TOUCH   1
    #define TOUCH_SDA   18
    #define TOUCH_SCL   17
    #define TOUCH_INT   16
    #define TOUCH_RST   21

#elif defined(BOARD_WAVESHARE_147)

    // Single BOOT button only (active LOW, internal pull-up)
    #define BTN_BOOT    0     // GPIO 0 = BOOT button
    #define HAS_TWO_BUTTONS   0

    // Backlight PWM
    #define PIN_BL      48
    #define BL_CHANNEL  0
    #define BL_FREQ     5000
    #define BL_RES      8     // 8-bit resolution (0-255)

    // No power pin needed
    #define HAS_POWER_PIN 0

    // No touch controller
    #define HAS_TOUCH   0

    // RGB LED (for optional status indication)
    #define PIN_RGB_LED 38

#elif defined(BOARD_C6_AMOLED)

    // Single BOOT button (active LOW, internal pull-up)
    // ESP32-C6 BOOT button is GPIO 9
    #define BTN_BOOT    9
    #define HAS_TWO_BUTTONS   0

    // AMOLED brightness — software controlled via SH8601, no PWM backlight pin
    #define HAS_BACKLIGHT_PIN 0
    #define BL_CHANNEL  0     // unused, kept for compilation compatibility
    #define BL_FREQ     5000
    #define BL_RES      8

    // No power control pin
    #define HAS_POWER_PIN 0

    // Capacitive touch (FT3168 via I2C)
    #define HAS_TOUCH   1
    #define TOUCH_SDA   8
    #define TOUCH_SCL   7
    #define TOUCH_INT   15

    // Display QSPI pins (SH8601 AMOLED)
    #define LCD_SCLK    0
    #define LCD_SDIO0   1
    #define LCD_SDIO1   2
    #define LCD_SDIO2   3
    #define LCD_SDIO3   4
    #define LCD_CS      5

    // Native portrait resolution (passed to Arduino_SH8601 constructor)
    // rotation=1 swaps these to landscape: 448x368
    #define LCD_WIDTH   368
    #define LCD_HEIGHT  448

    // I/O Expander (TCA9554) — controls display enable
    #define HAS_IO_EXPANDER 1
    #define IO_EXP_ADDR 0x20
    #define IO_EXP_PIN_LCD_EN1  4
    #define IO_EXP_PIN_LCD_EN2  5

    // AXP2101 Power Management IC (shared I2C bus with touch)
    #define HAS_PMU         1
    // XPOWERS_CHIP_AXP2101 defined via build_flags in platformio.ini
    #define PMU_I2C_ADDR    0x34

    // SD Card (optional)
    #define SD_CLK      11
    #define SD_CMD      10
    #define SD_DATA     18
    #define SD_CS       6

#else
    #error "No board defined! Add -DBOARD_TDISPLAY_S3, -DBOARD_WAVESHARE_147, or -DBOARD_C6_AMOLED to build_flags"
#endif
