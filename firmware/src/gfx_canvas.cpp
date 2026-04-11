#ifdef USE_ARDUINO_GFX

// ============================================================
// GfxCanvas — direct-to-display rendering for ESP32-C6 AMOLED
//
// The ESP32-C6 has 512KB SRAM with no PSRAM.  A full 448×368×2 = 330KB
// off-screen canvas buffer won't fit in the available heap alongside WiFi,
// FreeRTOS, and application data.
//
// Solution: route all drawing calls directly to the Arduino_SH8601 display.
// The SH8601 AMOLED has its own internal framebuffer; writes appear
// instantaneously and there is no visible tearing / flicker on AMOLED panels.
// pushSprite() is a deliberate no-op — content is already on-screen.
// ============================================================

#include "gfx_canvas.h"
#include <math.h>

GfxCanvas::GfxCanvas() {}

void GfxCanvas::setOutput(Arduino_GFX* output) {
    _output = output;
}

void GfxCanvas::setColorDepth(uint8_t depth) {
    // No off-screen buffer; colour depth is fixed by the display driver.
    (void)depth;
}

void* GfxCanvas::createSprite(int16_t w, int16_t h) {
    // No heap allocation — store dimensions and return non-null to indicate success.
    _width  = w;
    _height = h;
    return _output;  // caller only checks for null
}

void GfxCanvas::deleteSprite() {
    // Nothing allocated, nothing to free.
    _width  = 0;
    _height = 0;
}

// ---- Drawing Primitives (all route to the display directly) ----

void GfxCanvas::fillSprite(uint16_t color) {
    if (_output) _output->fillScreen(color);
}

void GfxCanvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (_output) _output->fillRect(x, y, w, h, color);
}

void GfxCanvas::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    if (_output) _output->fillRoundRect(x, y, w, h, r, color);
}

void GfxCanvas::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (_output) _output->fillCircle(x, y, r, color);
}

void GfxCanvas::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (_output) _output->drawFastHLine(x, y, w, color);
}

void GfxCanvas::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (_output) _output->drawFastVLine(x, y, h, color);
}

void GfxCanvas::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (_output) _output->drawLine(x0, y0, x1, y1, color);
}

void GfxCanvas::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (_output) _output->drawPixel(x, y, color);
}

// ---- Smooth Arc ----

void GfxCanvas::drawSmoothArc(int16_t cx, int16_t cy, int16_t orad, int16_t irad,
                               int16_t startAngle, int16_t endAngle,
                               uint16_t fg, uint16_t bg, bool roundEnds) {
    if (!_output) return;
    (void)bg;
    (void)roundEnds;

    while (startAngle < 0) startAngle += 360;
    while (endAngle   < 0) endAngle   += 360;
    startAngle %= 360;
    endAngle   %= 360;

    float startRad = startAngle * M_PI / 180.0f;
    float endRad   = endAngle   * M_PI / 180.0f;

    for (int16_t y = -orad; y <= orad; y++) {
        for (int16_t x = -orad; x <= orad; x++) {
            float dist = sqrtf((float)(x * x + y * y));
            if (dist < irad || dist > orad) continue;

            float angle = atan2f((float)x, (float)(-y));
            if (angle < 0) angle += 2.0f * M_PI;

            bool inSector;
            if (startRad <= endRad) {
                inSector = (angle >= startRad && angle <= endRad);
            } else {
                inSector = (angle >= startRad || angle <= endRad);
            }

            if (inSector) {
                _output->drawPixel(cx + x, cy + y, fg);
            }
        }
    }
}

// ---- Text Rendering ----

void GfxCanvas::loadFont(const uint8_t* font) {
    _currentFont = gfxFontLookup(font);
    if (_output && _currentFont) {
        _output->setFont(_currentFont);
    }
}

void GfxCanvas::unloadFont() {
    // GFX fonts are PROGMEM constants; nothing to release.
}

void GfxCanvas::setTextDatum(uint8_t datum) {
    _datum = datum;
}

void GfxCanvas::setTextColor(uint16_t fg, uint16_t bg) {
    _fgColor = fg;
    _bgColor = bg;
    if (_output) _output->setTextColor(fg, bg);
}

void GfxCanvas::adjustForDatum(const char* text, int16_t& x, int16_t& y) {
    if (!_output || !_currentFont || !text) return;

    int16_t  bx, by;
    uint16_t bw, bh;
    _output->getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);

    int16_t yAdv   = (int16_t)pgm_read_byte(&_currentFont->yAdvance);
    int16_t ascent = (yAdv * 3) / 4;

    switch (_datum) {
        case TL_DATUM:  y += ascent;                       break;
        case TC_DATUM:  x -= bw / 2;  y += ascent;        break;
        case TR_DATUM:  x -= bw;      y += ascent;        break;
        case ML_DATUM:  y += ascent - bh / 2;             break;
        case MC_DATUM:  x -= bw / 2;  y += ascent - bh / 2; break;
        case MR_DATUM:  x -= bw;      y += ascent - bh / 2; break;
        case BL_DATUM:  /* baseline already correct */     break;
        case BC_DATUM:  x -= bw / 2;                      break;
        case BR_DATUM:  x -= bw;                          break;
        default:        y += ascent;                       break;
    }
}

int16_t GfxCanvas::drawString(const char* text, int16_t x, int16_t y) {
    if (!_output || !text) return 0;

    if (_currentFont) {
        _output->setFont(_currentFont);
    }
    _output->setTextColor(_fgColor, _bgColor);

    int16_t drawX = x;
    int16_t drawY = y;
    adjustForDatum(text, drawX, drawY);

    _output->setCursor(drawX, drawY);
    _output->print(text);

    int16_t bx, by;
    uint16_t bw, bh;
    _output->getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);
    return (int16_t)bw;
}

int16_t GfxCanvas::drawString(const char* text, int16_t x, int16_t y, uint8_t font) {
    (void)font;
    return drawString(text, x, y);
}

// ---- Output ----

void GfxCanvas::pushSprite(int16_t x, int16_t y) {
    // No-op: all draw calls went directly to the display.
    (void)x;
    (void)y;
}

#endif // USE_ARDUINO_GFX
