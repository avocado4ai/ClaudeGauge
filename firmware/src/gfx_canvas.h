#pragma once

// ============================================================
// GfxCanvas — Graphics abstraction layer
//
// For TFT_eSPI boards:  GfxCanvas = TFT_eSprite (typedef, zero overhead)
// For Arduino_GFX boards: GfxCanvas wraps Arduino_Canvas with matching API
// ============================================================

#ifndef USE_ARDUINO_GFX

// ---- TFT_eSPI path (existing boards) ----
#include <TFT_eSPI.h>
using GfxCanvas = TFT_eSprite;

#else

// ---- Arduino_GFX path (ESP32-C6 AMOLED) ----
#include "Arduino_GFX_Library.h"
#include <Adafruit_GFX.h>
#include "gfx_fonts.h"

// Text datum constants (matching TFT_eSPI definitions)
#ifndef TL_DATUM
#define TL_DATUM 0  // Top-left
#define TC_DATUM 1  // Top-center
#define TR_DATUM 2  // Top-right
#define ML_DATUM 3  // Middle-left
#define MC_DATUM 4  // Middle-center
#define MR_DATUM 5  // Middle-right
#define BL_DATUM 6  // Bottom-left
#define BC_DATUM 7  // Bottom-center
#define BR_DATUM 8  // Bottom-right
#endif

class GfxCanvas {
public:
    // Constructor: takes the display output for pushSprite
    GfxCanvas();

    // Initialize with display output (called after display is created)
    void setOutput(Arduino_GFX* output);

    // Sprite lifecycle (matching TFT_eSprite API)
    void setColorDepth(uint8_t depth);
    void* createSprite(int16_t w, int16_t h);
    void deleteSprite();

    // Drawing primitives
    void fillSprite(uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    // Smooth arc (TFT_eSPI-specific, custom implementation for Arduino_GFX)
    void drawSmoothArc(int16_t cx, int16_t cy, int16_t orad, int16_t irad,
                       int16_t startAngle, int16_t endAngle,
                       uint16_t fg, uint16_t bg, bool roundEnds);

    // Text rendering (VLW-compatible API, uses GFX fonts internally)
    void loadFont(const uint8_t* font);
    void unloadFont();
    void setTextDatum(uint8_t datum);
    void setTextColor(uint16_t fg, uint16_t bg);
    int16_t drawString(const char* text, int16_t x, int16_t y);
    int16_t drawString(const char* text, int16_t x, int16_t y, uint8_t font);

    // Output
    void pushSprite(int16_t x, int16_t y);

    // Canvas dimensions
    int16_t width() const { return _width; }
    int16_t height() const { return _height; }

private:
    Arduino_GFX*     _output = nullptr;
    int16_t          _width = 0;
    int16_t          _height = 0;
    uint8_t          _datum = TL_DATUM;
    uint16_t         _fgColor = 0xFFFF;
    uint16_t         _bgColor = 0x0000;
    const GFXfont*   _currentFont = nullptr;

    // Adjust text position based on datum
    void adjustForDatum(const char* text, int16_t& x, int16_t& y);
};

#endif // USE_ARDUINO_GFX
