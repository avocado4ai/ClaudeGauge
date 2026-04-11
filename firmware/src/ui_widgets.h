#pragma once

#include "gfx_canvas.h"
#include "data_models.h"
#include <Arduino.h>

// Forward declarations only — definitions live in src/font_data.cpp
// (including the smooth_font_*.h headers in multiple translation units
//  causes linker "multiple definition" errors for the PROGMEM arrays)
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

// LCARS layout constants (landscape, dimensions from build flags)
#ifndef SCR_WIDTH
  #error "SCR_WIDTH must be defined via build flags in platformio.ini"
#endif
#ifndef SCR_HEIGHT
  #error "SCR_HEIGHT must be defined via build flags in platformio.ini"
#endif

#define SCR_W       SCR_WIDTH
#define SCR_H       SCR_HEIGHT

// Sidebar
#define SIDEBAR_W   34
#define ELBOW_R     14

// Bars
#define TOPBAR_H    18
#define BOTBAR_H    16
#define BAR_GAP     3

// Content area (inside the LCARS frame)
#define CONTENT_X   (SIDEBAR_W + ELBOW_R + 4)
#define CONTENT_Y   (TOPBAR_H + 12)
#define CONTENT_W   (SCR_W - CONTENT_X - 4)
#define CONTENT_H   (SCR_H - CONTENT_Y - BOTBAR_H - 4)

// Smooth font shortcuts (VLW arrays in PROGMEM)
#define LCARS_SM   SmoothFont12   // 12px: labels, status
#define LCARS_14   SmoothFont14   // 14px
#define LCARS_16   SmoothFont16   // 16px
#define LCARS_MD   SmoothFont18   // 18px: values, titles
#define LCARS_20   SmoothFont20   // 20px
#define LCARS_22   SmoothFont22   // 22px
#define LCARS_24   SmoothFont24   // 24px
#define LCARS_26   SmoothFont26   // 26px
#define LCARS_LG   SmoothFont28   // 28px: cost, big numbers
#define LCARS_XL   SmoothFont36   // 36px: primary cost

namespace UIWidgets {

    // ---- LCARS Frame ----
    void drawLcarsFrame(GfxCanvas& spr, const char* title,
                        uint8_t currentPage, uint8_t totalPages,
                        uint32_t countdownSec, bool wifiOk,
                        int16_t wifiRssi = -100);

    // ---- Data Widgets ----
    void drawProgressBar(GfxCanvas& spr, int16_t x, int16_t y,
                         int16_t w, int16_t h, float percent,
                         uint16_t fgColor, uint16_t bgColor);

    void drawTokenRow(GfxCanvas& spr, int16_t x, int16_t y,
                      int16_t barW, const char* label, uint64_t value,
                      uint64_t maxValue, uint16_t color);

    void drawSignalBars(GfxCanvas& spr, int16_t x, int16_t y,
                        int16_t rssi);

    void drawSeparator(GfxCanvas& spr, int16_t y);

    String formatCount(uint64_t value);

    void drawCostValue(GfxCanvas& spr, int16_t x, int16_t y,
                       float cost, const uint8_t* font, uint16_t color);

    void drawLabel(GfxCanvas& spr, int16_t x, int16_t y,
                   const char* text, uint16_t color,
                   const uint8_t* font = LCARS_SM);

    void drawAcceptBar(GfxCanvas& spr, int16_t x, int16_t y,
                       int16_t barW, const char* label,
                       uint16_t accepted, uint16_t rejected,
                       uint16_t color);

    void drawStatusRow(GfxCanvas& spr, int16_t x, int16_t y,
                       int16_t w, const char* label, const char* value,
                       uint16_t valueColor);

    void drawCostRow(GfxCanvas& spr, int16_t x, int16_t y,
                     int16_t barW, const char* label, float costUsd,
                     float maxCost, uint16_t color);

    void shortenModelName(const char* fullName, char* out, size_t outLen);
}
