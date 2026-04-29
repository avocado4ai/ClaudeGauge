#pragma once
#include <lcars.h>
#include "data_models.h"
#include "data_bindings.h"
#include "screen_layouts_v3.h"

class ClaudeAiScreen : public LcarsScreen {
public:
    const char* title() const override { return "CLAUDE.AI"; }
    uint32_t refreshIntervalMs() const override { return 1000; }  // 1s for ticking clock
    void setState(const AppState* s) { _state = s; }
    void invalidateGauges() { _gaugesDrawn = false; }

    void onDraw(LcarsCanvas& spr, const LcarsFrame::Rect& c) override {
        if (!_state) return;

        // No content clear — overdraw in place to avoid AMOLED flicker.
        // All elements redraw fully, covering their previous pixels.

        const ClaudeAiUsage& ai = _state->claude_ai;
        char pctBuf[16];

        // 5-hour window
#ifdef V3_AI_5H_LBL_X
        LcarsWidgets::drawLabel(spr, V3_AI_5H_LBL_X, V3_AI_5H_LBL_Y, "5-HOUR WINDOW", _theme->accent);
#endif
#ifdef V3_AI_5H_PCT_X
        snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", ai.five_hour.utilization);
        LcarsFont::drawText(spr, pctBuf, V3_AI_5H_PCT_X, V3_AI_5H_PCT_Y,
            LCARS_FONT_XL, _theme->text);
#endif
#ifdef V3_AI_5H_BAR_X
        LcarsWidgets::drawProgressBar(spr, V3_AI_5H_BAR_X, V3_AI_5H_BAR_Y,
            V3_AI_5H_BAR_W, 10, ai.five_hour.utilization / 100.0f,
            _theme->progressFg, _theme->progressBg);
#endif

        // Separator
#ifdef V3_AI_SEP_X
        LcarsWidgets::drawSeparator(spr, V3_AI_SEP_X, V3_AI_SEP_Y, V3_AI_SEP_W, _theme->accent);
#endif

        // 7-day window
#ifdef V3_AI_7D_LBL_X
        LcarsWidgets::drawLabel(spr, V3_AI_7D_LBL_X, V3_AI_7D_LBL_Y, "7-DAY WINDOW", _theme->accent);
#endif
#ifdef V3_AI_7D_PCT_X
        snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", ai.seven_day.utilization);
        LcarsFont::drawText(spr, pctBuf, V3_AI_7D_PCT_X, V3_AI_7D_PCT_Y,
            LCARS_FONT_LG, LCARS_ICE);
#endif
#ifdef V3_AI_7D_BAR_X
        LcarsWidgets::drawProgressBar(spr, V3_AI_7D_BAR_X, V3_AI_7D_BAR_Y,
            V3_AI_7D_BAR_W, 10, ai.seven_day.utilization / 100.0f,
            LCARS_ICE, _theme->progressBg);
#endif

        // Render data-bound custom elements (dragged from designer toolbox)
#if defined(V3_BOUND_SCREEN_0_COUNT) && V3_BOUND_SCREEN_0_COUNT > 0
        for (int i = 0; i < V3_BOUND_SCREEN_0_COUNT; i++) {
            const auto& be = V3_BOUND_SCREEN_0[i];
            float val = resolveBinding(be.bind, *_state);

            if (be.type == BTYPE_GAUGE) {
                // Only redraw gauges on first draw or data refresh (not every tick)
                if (!_gaugesDrawn) {
                    float pct = isPercentBinding(be.bind) ? val / 100.0f : 0;
                    char pctBuf[8];
                    snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", val);
                    LcarsWidgets::drawDonutGauge(spr, be.x, be.y, be.r, be.thk,
                        pct, _theme->progressFg, _theme->progressBg);
                    LcarsFont::drawText(spr, pctBuf, be.x, be.y,
                        LCARS_FONT_MD, _theme->text, _theme->background, MC_DATUM);
                }
            } else if (be.type == BTYPE_BAR) {
                if (!_gaugesDrawn) {
                    float pct = isPercentBinding(be.bind) ? val / 100.0f : 0;
                    LcarsWidgets::drawProgressBar(spr, be.x, be.y, be.w, be.h > 0 ? be.h : 10,
                        pct, _theme->progressFg, _theme->progressBg);
                }
            } else {
                // Text elements (clocks) — redraw every tick with bg clear
                LcarsFontSize fs = LCARS_FONT_MD;
                if (be.fontSize <= 12) fs = LCARS_FONT_SM;
                else if (be.fontSize <= 14) fs = LCARS_FONT_14;
                else if (be.fontSize <= 16) fs = LCARS_FONT_16;
                else if (be.fontSize <= 18) fs = LCARS_FONT_MD;
                else if (be.fontSize <= 20) fs = LCARS_FONT_20;
                else if (be.fontSize <= 28) fs = LCARS_FONT_LG;
                else fs = LCARS_FONT_XL;
                // Clear text area then redraw
                uint8_t fh = LcarsFont::getHeight(fs);
                spr.fillRect(be.x, be.y, 200, fh + 2, _theme->background);
                char buf[24];
                formatBinding(be.bind, val, buf, sizeof(buf));
                LcarsFont::drawText(spr, buf, be.x, be.y, fs, _theme->text);
            }
        }
        _gaugesDrawn = true;
#endif
    }

    void onSetup() override { _gaugesDrawn = false; }

private:
    const AppState* _state = nullptr;
    bool _gaugesDrawn = false;
};
