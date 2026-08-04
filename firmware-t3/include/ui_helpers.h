#pragma once
#include <TFT_eSPI.h>

// ============================================================
// LCARS Color Palette (RGB565) — shared between main.cpp and layout_engine.cpp
// ============================================================
#define C_BG         TFT_BLACK
#define C_TOPBAR     0x0010
#define C_BOTBAR     0x0010
#define C_WHITE      TFT_WHITE
#define C_GREEN      0x07E0
#define C_AMBER      0xFD09
#define C_TOMATO     0xF608
#define C_LAVENDER   0xB5B2
#define C_PEACH      0xFEE4
#define C_ICE        0xAEEF
#define C_TRACK      0x3186
#define C_DIM        0x632C
#define C_DIVIDER    0x2945
#define C_SEPARATOR  0x0018

// ============================================================
// Layout (240 x 135 landscape)
// ============================================================
#define TOPBAR_H  14
#define BOTBAR_H  13
#define CONTENT_Y TOPBAR_H
#define CONTENT_H (135 - TOPBAR_H - BOTBAR_H)

// Maps a widget "color" string (from layout.json) to an RGB565 value.
inline uint16_t uiColorFromName(const char* name, uint16_t fallback = C_WHITE) {
    if (!name || !name[0]) return fallback;
    if (!strcmp(name, "white"))    return C_WHITE;
    if (!strcmp(name, "green"))    return C_GREEN;
    if (!strcmp(name, "amber"))    return C_AMBER;
    if (!strcmp(name, "tomato"))   return C_TOMATO;
    if (!strcmp(name, "lavender")) return C_LAVENDER;
    if (!strcmp(name, "peach"))    return C_PEACH;
    if (!strcmp(name, "ice"))      return C_ICE;
    if (!strcmp(name, "dim"))      return C_DIM;
    return fallback;
}

inline void uiDrawDonut(TFT_eSPI* dp, int16_t cx, int16_t cy, int16_t r, int16_t t,
                         float pct, uint16_t color) {
    int16_t inner = r - t;
    if (inner < 2) inner = 2;
    dp->drawSmoothArc(cx, cy, r, inner, 0, 360, C_TRACK, C_BG, false);
    if (pct <= 0) return;
    if (pct > 1.0f) pct = 1.0f;
    int32_t endAngle = 270 + (int32_t)(360.0f * pct);
    if (endAngle > 630) endAngle = 630;
    dp->drawSmoothArc(cx, cy, r, inner, 270, endAngle, color, C_BG, false);
}

inline void uiDrawPctInside(TFT_eSPI* dp, int16_t cx, int16_t cy, float pct, uint16_t color) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f%%", pct);
    dp->setTextDatum(MC_DATUM);
    dp->setTextFont(4);
    dp->setTextColor(color, C_BG);
    dp->drawString(buf, cx, cy);
}

inline void uiDrawCountdown(TFT_eSPI* dp, uint32_t resets_at, int16_t cx, int16_t cy) {
    char buf[16];
    if (resets_at == 0) {
        snprintf(buf, sizeof(buf), "--:--");
    } else {
        int32_t rem = (int32_t)resets_at - (int32_t)time(nullptr);
        if (rem <= 0) { snprintf(buf, sizeof(buf), "0:00"); }
        else if (rem > 86400) {
            uint32_t d = rem / 86400, h = (rem % 86400) / 3600;
            snprintf(buf, sizeof(buf), "%lud %luh", (unsigned long)d, (unsigned long)h);
        } else {
            uint32_t h = rem / 3600, m = (rem % 3600) / 60, s = rem % 60;
            if (h > 0) snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)h, (unsigned long)m);
            else snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)m, (unsigned long)s);
        }
    }
    dp->setTextDatum(TC_DATUM);
    dp->setTextFont(4);
    dp->setTextColor(C_WHITE, C_BG);
    dp->drawString(buf, cx, cy);
}

inline void uiDrawHBar(TFT_eSPI* dp, int16_t x, int16_t y, int16_t w, int16_t h, float pct) {
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    int barW = (int)((w - 1) * pct / 100.0f);
    dp->drawRect(x, y, w, h, C_DIM);
    if (barW > 0) {
        uint16_t c = (pct > 80) ? C_TOMATO : (pct > 50) ? C_AMBER : C_GREEN;
        dp->fillRect(x + 1, y + 1, barW - 1, h - 2, c);
    }
}

inline void uiDraw7SegDigit(TFT_eSPI* dp, int16_t x, int16_t y, uint8_t d, uint16_t color) {
    static const uint8_t seg[10] = {
        0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
        0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111
    };
    uint8_t m = seg[d % 10];
    int16_t W = 26, H = 42, T = 4;
    if (m & 1)  dp->fillRect(x + T, y, W - T*2, T, color);
    if (m & 2)  dp->fillRect(x + W - T, y + T, T, H/2 - T, color);
    if (m & 4)  dp->fillRect(x + W - T, y + H/2, T, H/2 - T, color);
    if (m & 8)  dp->fillRect(x + T, y + H - T, W - T*2, T, color);
    if (m & 16) dp->fillRect(x, y + H/2, T, H/2 - T, color);
    if (m & 32) dp->fillRect(x, y + T, T, H/2 - T, color);
    if (m & 64) dp->fillRect(x + T, y + H/2 - T/2, W - T*2, T, color);
}
