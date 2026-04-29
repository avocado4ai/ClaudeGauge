#pragma once
#include <lcars.h>
#include "data_models.h"
#include "screen_layouts_v3.h"

class OverviewScreen : public LcarsScreen {
public:
    const char* title() const override { return "USAGE OVERVIEW"; }
    uint32_t refreshIntervalMs() const override { return 1000; }

    void setState(const AppState* s) { _state = s; }

    void onDraw(LcarsCanvas& spr, const LcarsFrame::Rect& c) override {
        if (!_state) return;

        if (_state->is_fetching) {
            LcarsFont::drawTextUpper(spr, "FETCHING DATA", c.x + c.w/2, c.y + c.h/2,
                LCARS_FONT_MD, _theme->accent, _theme->background, MC_DATUM);
            return;
        }

        // Today cost
#ifdef V3_OV_TODAY_X
        LcarsWidgets::drawLabel(spr, V3_OV_TODAY_X, V3_OV_TODAY_Y, "TODAY", _theme->accent);
#endif
#ifdef V3_OV_TODAYCOST_X
        char costBuf[16];
        snprintf(costBuf, sizeof(costBuf), "$%.2f", _state->cost.today_usd);
        LcarsFont::drawText(spr, costBuf, V3_OV_TODAYCOST_X, V3_OV_TODAYCOST_Y,
            LCARS_FONT_XL, _theme->text);
#endif

        // Monthly cost
#ifdef V3_OV_MONTH_X
        LcarsWidgets::drawLabel(spr, V3_OV_MONTH_X, V3_OV_MONTH_Y, "THIS MONTH", _theme->accent);
#endif
#ifdef V3_OV_MONTHCOST_X
        snprintf(costBuf, sizeof(costBuf), "$%.2f", _state->cost.month_usd);
        LcarsFont::drawText(spr, costBuf, V3_OV_MONTHCOST_X, V3_OV_MONTHCOST_Y,
            LCARS_FONT_LG, LCARS_ICE);
#endif

        // Separator
#ifdef V3_OV_SEP1_X
        LcarsWidgets::drawSeparator(spr, V3_OV_SEP1_X, V3_OV_SEP1_Y, V3_OV_SEP1_W, _theme->accent);
#endif

        // Cost breakdown header
#ifdef V3_OV_BRKDWN_X
        LcarsWidgets::drawLabel(spr, V3_OV_BRKDWN_X, V3_OV_BRKDWN_Y, "COST BREAKDOWN", _theme->accent);
#endif

        // Cost breakdown bars (use sequence macros if available)
#if defined(V3_OV_BAR0_X)
        if (_state->cost.model_count > 0) {
            float maxCost = 0.01f;
            int maxD = min((int)_state->cost.model_count, 4);
            for (int i = 0; i < maxD; i++)
                if (_state->cost.models[i].cost_usd > maxCost)
                    maxCost = _state->cost.models[i].cost_usd;

#ifdef V3_OV_BAR_START_Y
            // Use sequence macros for looped rendering
            for (int i = 0; i < maxD; i++) {
                float pct = _state->cost.models[i].cost_usd / maxCost;
                int16_t barY = V3_OV_BAR_START_Y + i * V3_OV_BAR_ROW_STEP;
                LcarsWidgets::drawProgressBar(spr, V3_OV_BAR0_X, barY, V3_OV_BAR0_W, 8, pct,
                    _theme->progressFg, _theme->progressBg);
            }
#endif
#ifdef V3_OV_MODEL_START_Y
            for (int i = 0; i < maxD; i++) {
                int16_t lblY = V3_OV_MODEL_START_Y + i * V3_OV_MODEL_ROW_STEP;
                LcarsFont::drawText(spr, _state->cost.models[i].model_name, V3_OV_MODEL0_X, lblY,
                    LCARS_FONT_SM, _theme->textDim);
            }
#endif
        }
#endif

        // Donut gauges
#ifdef V3_OV_G1_X
        if (_state->usage.valid) {
            uint64_t total = _state->usage.today_total.total();
            float inputPct = total > 0 ? (float)_state->usage.today_total.uncached_input / total : 0;
            LcarsWidgets::drawDonutGauge(spr, V3_OV_G1_X, V3_OV_G1_Y,
                V3_OV_G1_W / 2, V3_OV_G1_T, inputPct, _theme->progressFg, _theme->progressBg);
        }
#endif
#ifdef V3_OV_G1LBL_X
        LcarsFont::drawTextUpper(spr, "INPUT", V3_OV_G1LBL_X, V3_OV_G1LBL_Y,
            LCARS_FONT_SM, _theme->textDim, _theme->background, MC_DATUM);
#endif
#ifdef V3_OV_G2_X
        if (_state->usage.valid) {
            uint64_t total = _state->usage.today_total.total();
            float outputPct = total > 0 ? (float)_state->usage.today_total.output / total : 0;
            LcarsWidgets::drawDonutGauge(spr, V3_OV_G2_X, V3_OV_G2_Y,
                V3_OV_G2_W / 2, V3_OV_G2_T, outputPct, LCARS_ICE, _theme->progressBg);
        }
#endif
#ifdef V3_OV_G2LBL_X
        LcarsFont::drawTextUpper(spr, "OUTPUT", V3_OV_G2LBL_X, V3_OV_G2LBL_Y,
            LCARS_FONT_SM, _theme->textDim, _theme->background, MC_DATUM);
#endif

        // Custom elements
#if defined(V3_CUSTOM_SCREEN_0_COUNT) && V3_CUSTOM_SCREEN_0_COUNT > 0
        for (int i = 0; i < V3_CUSTOM_SCREEN_0_COUNT; i++) {
            LcarsFont::drawText(spr, V3_CUSTOM_SCREEN_0[i].text,
                V3_CUSTOM_SCREEN_0[i].x, V3_CUSTOM_SCREEN_0[i].y,
                LCARS_FONT_SM, _theme->text);
        }
#endif
    }

private:
    const AppState* _state = nullptr;
};
