#pragma once

// ============================================================
// GFX Font system for Arduino_GFX boards (ESP32-C6 AMOLED etc.)
// Maps VLW smooth font pointers to Adafruit GFX bitmap fonts.
// Only compiled when USE_ARDUINO_GFX is defined.
// ============================================================

#ifdef USE_ARDUINO_GFX

#include <Adafruit_GFX.h>
#include "gfx_font_12.h"
#include "gfx_font_14.h"
#include "gfx_font_16.h"
#include "gfx_font_18.h"
#include "gfx_font_20.h"
#include "gfx_font_22.h"
#include "gfx_font_24.h"
#include "gfx_font_26.h"
#include "gfx_font_28.h"
#include "gfx_font_36.h"

// Font size lookup table: maps VLW font pointer → GFX font pointer.
// Since VLW fonts won't be loaded on Arduino_GFX builds, we use
// the SmoothFontXX symbols as keys and map them to AntonioGFXXX fonts.
// The GfxCanvas::loadFont() method performs this mapping at runtime.

struct GfxFontMapping {
    const uint8_t* vlwFont;      // VLW font pointer (from smooth_font_*.h)
    const GFXfont* gfxFont;      // Corresponding GFX font
    int16_t        pixelSize;    // Font size for metrics
};

// Forward-declared in smooth_font_*.h — these are the VLW arrays.
// On USE_ARDUINO_GFX builds we still include the smooth_font headers
// (they're tiny references), but loadFont maps them to GFX fonts.
extern const uint8_t SmoothFont12[];
extern const uint8_t SmoothFont14[];
extern const uint8_t SmoothFont16[];
extern const uint8_t SmoothFont18[];
extern const uint8_t SmoothFont20[];
extern const uint8_t SmoothFont22[];
extern const uint8_t SmoothFont24[];
extern const uint8_t SmoothFont26[];
extern const uint8_t SmoothFont28[];
extern const uint8_t SmoothFont36[];

inline const GFXfont* gfxFontLookup(const uint8_t* vlwFont) {
    // Map VLW font pointer to GFX font by identity comparison
    if (vlwFont == SmoothFont12) return &AntonioGFX12;
    if (vlwFont == SmoothFont14) return &AntonioGFX14;
    if (vlwFont == SmoothFont16) return &AntonioGFX16;
    if (vlwFont == SmoothFont18) return &AntonioGFX18;
    if (vlwFont == SmoothFont20) return &AntonioGFX20;
    if (vlwFont == SmoothFont22) return &AntonioGFX22;
    if (vlwFont == SmoothFont24) return &AntonioGFX24;
    if (vlwFont == SmoothFont26) return &AntonioGFX26;
    if (vlwFont == SmoothFont28) return &AntonioGFX28;
    if (vlwFont == SmoothFont36) return &AntonioGFX36;
    return &AntonioGFX12;  // fallback
}

#endif // USE_ARDUINO_GFX
