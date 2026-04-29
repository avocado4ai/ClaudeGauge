#pragma once

// ============================================================
// Pin configuration — Waveshare ESP32-C6-Touch-AMOLED-1.8
// ============================================================

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
