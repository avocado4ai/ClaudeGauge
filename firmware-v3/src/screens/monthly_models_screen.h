#pragma once
#include <lcars.h>
#include "data_models.h"
#include "screen_layouts_v3.h"

class MonthlyModelsScreen : public LcarsScreen {
public:
    const char* title() const override { return "MONTHLY MODELS"; }
    uint32_t refreshIntervalMs() const override { return 1000; }
    void setState(const AppState* s) { _state = s; }

    void onDraw(LcarsCanvas& spr, const LcarsFrame::Rect& c) override {
        if (!_state) return;
        if (_state->is_fetching) {
            LcarsFont::drawTextUpper(spr, "FETCHING...", c.x + c.w/2, c.y + c.h/2,
                LCARS_FONT_MD, _theme->accent, _theme->background, MC_DATUM);
            return;
        }

        const UsageData& usage = _state->monthly_usage;
        if (!usage.valid || usage.model_count == 0) {
            LcarsFont::drawTextUpper(spr, "NO MODEL DATA", c.x + c.w/2, c.y + c.h/2,
                LCARS_FONT_MD, _theme->accent, _theme->background, MC_DATUM);
            return;
        }

        // Reuses same layout macros as Today Models
#if defined(V3_MOD_N_START_Y) && defined(V3_MOD_B_START_Y)
        uint64_t maxTotal = 1;
        int maxD = min((int)usage.model_count, 6);
        for (int i = 0; i < maxD; i++) {
            uint64_t t = usage.models[i].tokens.total();
            if (t > maxTotal) maxTotal = t;
        }

        for (int i = 0; i < maxD; i++) {
            const auto& m = usage.models[i];
            uint64_t total = m.tokens.total();
            float pct = (float)total / maxTotal;
            int16_t ny = V3_MOD_N_START_Y + i * V3_MOD_N_ROW_STEP;
            int16_t by = V3_MOD_B_START_Y + i * V3_MOD_B_ROW_STEP;

            LcarsFont::drawText(spr, m.model_name, V3_MOD_N0_X, ny,
                LCARS_FONT_SM, _theme->text);
            char valBuf[16];
            LcarsWidgets::formatCount(total, valBuf, sizeof(valBuf));
            LcarsFont::drawText(spr, valBuf, V3_MOD_V0_X, ny,
                LCARS_FONT_SM, _theme->accent, _theme->background, TR_DATUM);
            LcarsWidgets::drawProgressBar(spr, V3_MOD_B0_X, by, V3_MOD_B0_W, 6, pct,
                _theme->progressFg, _theme->progressBg);
        }
#endif
    }

private:
    const AppState* _state = nullptr;
};
