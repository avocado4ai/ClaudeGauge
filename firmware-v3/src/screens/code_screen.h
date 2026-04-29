#pragma once
#include <lcars.h>
#include "data_models.h"
#include "screen_layouts_v3.h"

class CodeScreen : public LcarsScreen {
public:
    const char* title() const override { return "TODAY CODE"; }
    uint32_t refreshIntervalMs() const override { return 1000; }
    void setState(const AppState* s) { _state = s; }

    void onDraw(LcarsCanvas& spr, const LcarsFrame::Rect& c) override {
        if (!_state) return;
        if (_state->is_fetching) {
            LcarsFont::drawTextUpper(spr, "FETCHING...", c.x + c.w/2, c.y + c.h/2,
                LCARS_FONT_MD, _theme->accent, _theme->background, MC_DATUM);
            return;
        }
        _drawCode(spr, c, _state->code);
    }

private:
    const AppState* _state = nullptr;

    void _drawCode(LcarsCanvas& spr, const LcarsFrame::Rect& c, const ClaudeCodeData& code) {
        if (!code.valid) {
            LcarsFont::drawTextUpper(spr, "NO CODE DATA", c.x + c.w/2, c.y + c.h/2,
                LCARS_FONT_MD, _theme->accent, _theme->background, MC_DATUM);
            return;
        }

        char buf[24];

#ifdef V3_CC_SESS_L_X
        LcarsWidgets::drawLabel(spr, V3_CC_SESS_L_X, V3_CC_SESS_L_Y, "SESSIONS", _theme->accent);
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)code.total_sessions);
        LcarsFont::drawText(spr, buf, V3_CC_SESS_V_X, V3_CC_SESS_V_Y,
            LCARS_FONT_MD, _theme->text, _theme->background, TR_DATUM);
#endif

#ifdef V3_CC_LINES_L_X
        LcarsWidgets::drawLabel(spr, V3_CC_LINES_L_X, V3_CC_LINES_L_Y, "LINES", _theme->accent);
        snprintf(buf, sizeof(buf), "+%ld / -%ld",
            (long)code.total_lines_added, (long)code.total_lines_removed);
        LcarsFont::drawText(spr, buf, V3_CC_LINES_V_X, V3_CC_LINES_V_Y,
            LCARS_FONT_SM, LCARS_ICE, _theme->background, TR_DATUM);
#endif

#ifdef V3_CC_COMMITS_X
        LcarsWidgets::drawStatusRow(spr, V3_CC_COMMITS_X, V3_CC_COMMITS_Y, V3_CC_COMMITS_W,
            "COMMITS", String(code.total_commits).c_str(), _theme->accent, _theme->textDim);
#endif

#ifdef V3_CC_PRS_X
        LcarsWidgets::drawStatusRow(spr, V3_CC_PRS_X, V3_CC_PRS_Y, V3_CC_PRS_W,
            "PULL REQS", String(code.total_prs).c_str(), _theme->accent, _theme->textDim);
#endif

#ifdef V3_CC_SEP_X
        LcarsWidgets::drawSeparator(spr, V3_CC_SEP_X, V3_CC_SEP_Y, V3_CC_SEP_W, _theme->accent);
#endif

#ifdef V3_CC_COST_L_X
        LcarsWidgets::drawLabel(spr, V3_CC_COST_L_X, V3_CC_COST_L_Y, "EST. COST", _theme->accent);
        snprintf(buf, sizeof(buf), "$%.2f", code.total_cost);
        LcarsFont::drawText(spr, buf, V3_CC_COST_V_X, V3_CC_COST_V_Y,
            LCARS_FONT_MD, LCARS_ICE, _theme->background, TR_DATUM);
#endif

#ifdef V3_CC_ACC_L_X
        uint16_t totalEdits = code.total_edit_accepted + code.total_edit_rejected;
        if (totalEdits > 0) {
            LcarsWidgets::drawLabel(spr, V3_CC_ACC_L_X, V3_CC_ACC_L_Y, "EDIT ACCEPTANCE", _theme->accent);
            float acceptPct = (float)code.total_edit_accepted / totalEdits;
            LcarsWidgets::drawProgressBar(spr, V3_CC_ACC_BAR_X, V3_CC_ACC_BAR_Y,
                V3_CC_ACC_BAR_W, 8, acceptPct, _theme->progressFg, _theme->progressBg);
            snprintf(buf, sizeof(buf), "%u/%u", code.total_edit_accepted, totalEdits);
            LcarsFont::drawText(spr, buf, V3_CC_ACC_V_X, V3_CC_ACC_V_Y,
                LCARS_FONT_SM, _theme->textDim, _theme->background, TR_DATUM);
        }
#endif
    }
};
