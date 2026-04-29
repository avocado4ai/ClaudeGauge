#pragma once
#include <lcars.h>

class ConnectingScreen : public LcarsScreen {
public:
    const char* title() const override { return "CONNECTING"; }
    uint32_t refreshIntervalMs() const override { return 200; }

    void setStatus(const char* s) { strncpy(_status, s, sizeof(_status) - 1); }

    void onDraw(LcarsCanvas& spr, const LcarsFrame::Rect& c) override {
        int16_t cx = c.x + c.w / 2;
        int16_t cy = c.y + c.h / 2;

        LcarsFont::drawTextUpper(spr, "INITIALIZING", cx, cy - 30,
            LCARS_FONT_LG, _theme->accent, _theme->background, MC_DATUM);
        LcarsFont::drawText(spr, _status, cx, cy + 10,
            LCARS_FONT_MD, _theme->textDim, _theme->background, MC_DATUM);

        // Animated dots
        uint8_t dots = (millis() / 500) % 4;
        for (int i = 0; i < (int)dots; i++) {
            spr.fillRoundRect(cx - 45 + i * 30, cy + 40, 18, 10, 5, _theme->accent);
        }
    }

private:
    char _status[48] = "";
};
