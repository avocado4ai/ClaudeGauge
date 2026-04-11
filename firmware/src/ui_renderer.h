#pragma once

#include "gfx_canvas.h"
#include "data_models.h"

namespace UIRenderer {
    void drawOverview(GfxCanvas& spr, const AppState& state, uint32_t countdown);
    void drawModels(GfxCanvas& spr, const AppState& state, uint32_t countdown);
    void drawMonthlyModels(GfxCanvas& spr, const AppState& state, uint32_t countdown);
    void drawClaudeCode(GfxCanvas& spr, const AppState& state, uint32_t countdown);
    void drawMonthlyCode(GfxCanvas& spr, const AppState& state, uint32_t countdown);
    void drawStatus(GfxCanvas& spr, const AppState& state, uint32_t countdown);
    void drawClaudeAi(GfxCanvas& spr, const AppState& state, uint32_t countdown);
}
