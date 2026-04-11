#pragma once

#include "gfx_canvas.h"
#include "data_models.h"

class DisplayManager {
public:
    void init();
    void showSplash();
    void showConnecting(const char* status);
    void showSetupScreen(const char* apName, const char* ip);
    void setBacklight(uint8_t level);

    GfxCanvas& sprite() { return _sprite; }

    void pushSprite();

private:
#ifndef USE_ARDUINO_GFX
    // ---- TFT_eSPI path (existing boards) ----
    TFT_eSPI    _tft;
    GfxCanvas   _sprite = GfxCanvas(&_tft);
#else
    // ---- Arduino_GFX path (ESP32-C6 AMOLED) ----
    Arduino_DataBus*  _bus     = nullptr;
    Arduino_SH8601*   _display = nullptr;
    GfxCanvas         _sprite;
#endif
    uint8_t _blLevel = 255;
};
