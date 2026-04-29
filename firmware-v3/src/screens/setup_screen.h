#pragma once
#include <lcars.h>

class SetupScreen : public LcarsScreen {
public:
    const char* title() const override { return "SETUP REQUIRED"; }
    uint32_t refreshIntervalMs() const override { return 500; }

    void setAPName(const char* name) { strncpy(_apName, name, sizeof(_apName) - 1); }
    void setIP(const char* ip) { snprintf(_url, sizeof(_url), "http://%s", ip); }

    void onDraw(LcarsCanvas& spr, const LcarsFrame::Rect& c) override {
        int16_t cx = c.x + c.w / 2;
        int16_t y = c.y + 20;

        LcarsFont::drawTextUpper(spr, "CONNECT TO WIFI:", cx, y,
            LCARS_FONT_MD, _theme->text, _theme->background, MC_DATUM);
        y += 30;
        LcarsFont::drawText(spr, _apName, cx, y,
            LCARS_FONT_LG, _theme->accent, _theme->background, MC_DATUM);
        y += 40;
        LcarsFont::drawTextUpper(spr, "THEN OPEN:", cx, y,
            LCARS_FONT_MD, _theme->text, _theme->background, MC_DATUM);
        y += 30;
        LcarsFont::drawText(spr, _url, cx, y,
            LCARS_FONT_20, _theme->accent, _theme->background, MC_DATUM);
    }

private:
    char _apName[32] = "ClaudeGauge";
    char _url[40] = "http://192.168.4.1";
};
