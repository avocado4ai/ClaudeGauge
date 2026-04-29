#include "display_manager.h"
#include "pin_config.h"
#include "colors.h"
#include "config.h"
#include "ui_widgets.h"

#ifdef USE_ARDUINO_GFX
  #include <Wire.h>
  #include <Adafruit_XCA9554.h>
  #ifdef XPOWERS_CHIP_AXP2101
    #include <XPowersLib.h>
  #endif
#endif

// Local helper: draw string with smooth font
static void lcarsText(GfxCanvas& spr, const char* text, int16_t x, int16_t y,
                      const uint8_t* font, uint16_t color,
                      uint8_t datum = TL_DATUM) {
    spr.loadFont(font);
    spr.setTextDatum(datum);
    spr.setTextColor(color, CLR_BG);
    spr.drawString(text, x, y);
    spr.unloadFont();
}

void DisplayManager::init() {

#ifndef USE_ARDUINO_GFX
    // ================================================================
    // TFT_eSPI path (existing boards — T-Display-S3, Waveshare 1.47)
    // ================================================================
    #if HAS_POWER_PIN
    // Power on the display (T-Display-S3 specific)
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    #endif

    _tft.init();
    _tft.invertDisplay(true);  // ST7789 panels need inversion for correct colors
    _tft.setRotation(1);       // Landscape

    _tft.fillScreen(CLR_BG);

    // Create full-screen sprite in PSRAM for flicker-free updates
    _sprite.setColorDepth(16);
    _sprite.createSprite(SCR_W, SCR_H);
    _sprite.fillSprite(CLR_BG);

    // Backlight via LEDC PWM
    ledcSetup(BL_CHANNEL, BL_FREQ, BL_RES);
    ledcAttachPin(PIN_BL, BL_CHANNEL);
    setBacklight(BACKLIGHT_FULL);

#else
    // ================================================================
    // Arduino_GFX path (ESP32-C6 AMOLED — SH8601 via QSPI)
    // ================================================================

    // Initialize I2C for touch + I/O expander + PMU
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    // Initialize AXP2101 PMIC — configure charging only.
    // DO NOT reconfigure power rails (DC1, BLDO1, etc.) — the factory OTP
    // defaults are correct. Changing them can brick the board.
    #ifdef XPOWERS_CHIP_AXP2101
    {
        XPowersPMU pmu;
        if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, TOUCH_SDA, TOUCH_SCL)) {
            Serial.println("[PMU] AXP2101 found");

            // Battery charging — safe to configure, doesn't affect running rails.
            // These are the EXACT settings proven to work on hardware via the
            // test_battery sketch. DO NOT add setDC*/enableDC*/setBLDO*/enableBLDO*
            // calls — those will brick the board.
            pmu.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
            pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
            pmu.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
            pmu.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

            // Enable read-only ADC measurements (safe)
            pmu.enableBattDetection();
            pmu.enableBattVoltageMeasure();
            pmu.enableVbusVoltageMeasure();
            pmu.enableSystemVoltageMeasure();
            pmu.enableTemperatureMeasure();

            Serial.printf("[PMU] Battery voltage: %dmV\n", pmu.getBattVoltage());
        } else {
            Serial.println("[PMU] AXP2101 not found — running on USB power only");
        }
    }
    #endif

    // Initialize TCA9554 I/O expander — must enable display before any QSPI
    Adafruit_XCA9554 expander;
    if (expander.begin(IO_EXP_ADDR, &Wire)) {
        expander.pinMode(IO_EXP_PIN_LCD_EN1, OUTPUT);
        expander.pinMode(IO_EXP_PIN_LCD_EN2, OUTPUT);
        expander.digitalWrite(IO_EXP_PIN_LCD_EN1, HIGH);
        expander.digitalWrite(IO_EXP_PIN_LCD_EN2, HIGH);
    }

    // Create QSPI bus for SH8601 AMOLED
    _bus = new Arduino_ESP32QSPI(
        LCD_CS,    // CS
        LCD_SCLK,  // SCK
        LCD_SDIO0, // SDIO0
        LCD_SDIO1, // SDIO1
        LCD_SDIO2, // SDIO2
        LCD_SDIO3  // SDIO3
    );

    // Create SH8601 display driver
    // Native resolution: 368x448 (portrait).
    // NOTE: The SH8601 does NOT support hardware rotation via MADCTL — all
    // setRotation() cases send the same register value.  Using rotation=1
    // causes the software to treat the display as 448x368, but CASET still
    // only goes to 368 physical columns, so fillScreen() only covers ~80% of
    // the panel and the rest stays as GRAM noise.  Use rotation=0 so that
    // software coordinates exactly match the physical column/row count.
    _display = new Arduino_SH8601(
        _bus,
        GFX_NOT_DEFINED,  // RST pin (via expander, not GPIO)
        0,                // Rotation=0: portrait, matches physical 368x448
        LCD_WIDTH,        // 368 (native portrait width)
        LCD_HEIGHT        // 448 (native portrait height)
    );

    _display->begin();
    _display->fillScreen(CLR_BG);

    // GfxCanvas routes all drawing directly to the display on this board
    // (ESP32-C6 has 512KB SRAM — no room for a 448×368×2=330KB canvas buffer).
    _sprite.setOutput(_display);
    _sprite.createSprite(SCR_W, SCR_H);  // stores dimensions, no heap alloc

    // AMOLED brightness (software controlled)
    _display->setBrightness(255);

#endif
}

void DisplayManager::showSplash() {
    UIWidgets::drawLcarsFrame(_sprite, "LCARS v2.0", 0, 1, 0, false);

    int16_t cx = CONTENT_X + CONTENT_W / 2;
    int16_t cy = CONTENT_Y + CONTENT_H / 2;

#if SCR_H >= 300  // Tall portrait screen (C6 AMOLED 448px)
    lcarsText(_sprite, "CLAUDE", cx, cy - 50, LCARS_XL, CLR_PEACH, MC_DATUM);
    lcarsText(_sprite, "USAGE MONITOR", cx, cy + 4, LCARS_LG, CLR_PEACH, MC_DATUM);
    lcarsText(_sprite, "INITIALIZING...", cx, cy + 50, LCARS_MD, CLR_LAVENDER, MC_DATUM);
#else
    lcarsText(_sprite, "CLAUDE", cx, cy - 28, LCARS_LG, CLR_PEACH, MC_DATUM);
    lcarsText(_sprite, "USAGE MONITOR", cx, cy + 4, LCARS_LG, CLR_PEACH, MC_DATUM);
    lcarsText(_sprite, "INITIALIZING...", cx, cy + 34, LCARS_SM, CLR_LAVENDER, MC_DATUM);
#endif

    pushSprite();
}

void DisplayManager::showConnecting(const char* status) {
    UIWidgets::drawLcarsFrame(_sprite, "CONNECTING", 0, 1, 0, false);

    int16_t cx = CONTENT_X + CONTENT_W / 2;
    int16_t cy = CONTENT_Y + CONTENT_H / 2;

#if SCR_H >= 300  // Tall portrait screen (C6 AMOLED 448px)
    lcarsText(_sprite, "INITIALIZING", cx, cy - 36, LCARS_LG, CLR_PEACH, MC_DATUM);
    lcarsText(_sprite, status, cx, cy + 10, LCARS_MD, CLR_LAVENDER, MC_DATUM);
    static uint8_t dotCount = 0;
    dotCount = (dotCount + 1) % 4;
    int16_t dotY = cy + 46;
    for (int i = 0; i < dotCount; i++) {
        _sprite.fillRoundRect(cx - 45 + i * 30, dotY, 18, 10, 5, CLR_AMBER);
    }
#else
    lcarsText(_sprite, "INITIALIZING", cx, cy - 22, LCARS_LG, CLR_PEACH, MC_DATUM);
    lcarsText(_sprite, status, cx, cy + 12, LCARS_MD, CLR_LAVENDER, MC_DATUM);
    static uint8_t dotCount = 0;
    dotCount = (dotCount + 1) % 4;
    int16_t dotY = cy + 30;
    for (int i = 0; i < dotCount; i++) {
        _sprite.fillRoundRect(cx - 30 + i * 20, dotY, 12, 8, 4, CLR_AMBER);
    }
#endif

    pushSprite();
}

void DisplayManager::showSetupScreen(const char* apName, const char* ip) {
    UIWidgets::drawLcarsFrame(_sprite, "SETUP REQUIRED", 0, 1, 0, false);

    char url[40];
    snprintf(url, sizeof(url), "http://%s", ip);

#if SCR_H >= 300  // Tall portrait screen (C6 AMOLED 448px)
    // Vertically center content block in the large content area.
    // Block height: label(24) + gap(16) + ssid(36) + gap(32) + label(24) + gap(16) + url(28) = 176px
    const int16_t blockH = 24 + 16 + 36 + 32 + 24 + 16 + 28;
    int16_t x = CONTENT_X + 8;
    int16_t y = CONTENT_Y + (CONTENT_H - blockH) / 2;

    lcarsText(_sprite, "Connect to WiFi:", x, y, LCARS_24, CLR_LAVENDER);
    y += 24 + 16;
    lcarsText(_sprite, apName, x, y, LCARS_XL, CLR_AMBER);
    y += 36 + 32;
    lcarsText(_sprite, "Then open:", x, y, LCARS_24, CLR_LAVENDER);
    y += 24 + 16;
    lcarsText(_sprite, url, x, y, LCARS_LG, CLR_PEACH);
#else
    int16_t x = CONTENT_X;
    int16_t y = CONTENT_Y;
    lcarsText(_sprite, "Connect to WiFi:", x, y, LCARS_SM, CLR_LAVENDER);
    y += 18;
    lcarsText(_sprite, apName, x, y, LCARS_MD, CLR_AMBER);
    y += 28;
    lcarsText(_sprite, "Then open:", x, y, LCARS_SM, CLR_LAVENDER);
    y += 18;
    lcarsText(_sprite, url, x, y, LCARS_MD, CLR_PEACH);
#endif

    pushSprite();
}

void DisplayManager::setBacklight(uint8_t level) {
    _blLevel = level;
#ifndef USE_ARDUINO_GFX
    ledcWrite(BL_CHANNEL, level);
#else
    if (_display) _display->setBrightness(level);
#endif
}

void DisplayManager::pushSprite() {
    _sprite.pushSprite(0, 0);
}
