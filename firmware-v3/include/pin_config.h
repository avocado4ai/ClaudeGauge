#pragma once

// ============================================================
// Board-specific pin configuration
// ============================================================

#if defined(BOARD_C6_AMOLED)

    // Pin configuration — Waveshare ESP32-C6-Touch-AMOLED-1.8

    // Button
    #define BTN_BOOT    9
    #define HAS_TWO_BUTTONS   0

    // Display — SH8601 AMOLED via QSPI
    #define LCD_CS      5
    #define LCD_SCLK    0
    #define LCD_SDIO0   1
    #define LCD_SDIO1   2
    #define LCD_SDIO2   3
    #define LCD_SDIO3   4
    #define LCD_WIDTH   368
    #define LCD_HEIGHT  448

    // I2C (touch + I/O expander + PMU)
    #define TOUCH_SDA   8
    #define TOUCH_SCL   7

    // I/O Expander (TCA9554)
    #define IO_EXP_ADDR          0x20
    #define IO_EXP_PIN_LCD_EN1   4
    #define IO_EXP_PIN_LCD_EN2   5

    // Touch (FT3168)
    #define HAS_TOUCH   1
    #define TOUCH_INT   15
    #define TOUCH_ADDR  0x38

    // No backlight pin (AMOLED software-controlled brightness)
    #define HAS_BACKLIGHT_PIN 0
    #define HAS_POWER_PIN     0

#elif defined(BOARD_TDISPLAY_S3) || defined(BOARD_TDISPLAY_S3_NOTOUCH)

    // Pin configuration — LILYGO T-Display-S3 (TFT_eSPI, ST7789 parallel)

    // Buttons (active LOW, internal pull-up)
    #define BTN_LEFT    0     // GPIO 0  = BOOT button
    #define BTN_RIGHT   14    // GPIO 14 = second button
    #define HAS_TWO_BUTTONS   1

    // Backlight (driven via engine.setBLPin — no PWM setup needed here)
    #define PIN_BL      38
    #define HAS_BACKLIGHT_PIN 1

    // Power control (T-Display-S3 specific)
    #define PIN_POWER_ON 15
    #define HAS_POWER_PIN 1

    // Capacitive touch (CST816S via I2C) — not present on NOTOUCH variant
    #if defined(BOARD_TDISPLAY_S3_NOTOUCH)
        #define HAS_TOUCH   0
    #else
        #define HAS_TOUCH   1
        #define TOUCH_SDA   18
        #define TOUCH_SCL   17
        #define TOUCH_INT   16
        #define TOUCH_RST   21
    #endif

#else
    #error "No board defined! Add -DBOARD_C6_AMOLED, -DBOARD_TDISPLAY_S3, or -DBOARD_TDISPLAY_S3_NOTOUCH to build_flags"
#endif
