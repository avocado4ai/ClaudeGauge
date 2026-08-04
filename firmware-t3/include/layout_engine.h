#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <vector>

// ============================================================
// Runtime, JSON-driven page/widget model.
//
// Replaces the hardcoded screenMain()/screenOpusSonnet()/... dispatch with a
// generic renderer driven by /layout.json (stored on LittleFS). Widget
// "bind" values are resolved against LayoutContext, a snapshot of the
// existing runtime globals populated once per frame in main.cpp.
// ============================================================

namespace layoutEngine {

enum WidgetType {
    WIDGET_DONUT_GAUGE,
    WIDGET_BAR,
    WIDGET_TEXT,
    WIDGET_CLOCK7SEG,
    WIDGET_UNKNOWN
};

struct Widget {
    WidgetType type = WIDGET_UNKNOWN;
    int16_t x = 0, y = 0, w = 0, h = 0;
    int16_t r = 34, thickness = 7;
    String bind;      // data source key, resolved against LayoutContext
    String color;     // named color, see uiColorFromName()
    String label;      // static label text (used by WIDGET_TEXT and gauge labels)
    uint8_t font = 2;  // TFT_eSPI font id, used by WIDGET_TEXT
};

struct Page {
    String id;
    bool enabled = true;
    std::vector<Widget> widgets;
};

struct GpuSnapshot {
    float utilization = 0, memUsed = 0, memTotal = 0, temp = 0;
    char name[24] = "";
};

// Snapshot of runtime state widgets can bind to. Populated once per frame by
// main.cpp before calling renderPage(), so the layout engine never touches
// the global state directly.
struct LayoutContext {
    float limit5hPct = 0, limit7dPct = 0, limitOpusPct = 0, limitSonnetPct = 0;
    uint32_t limit5hResetsAt = 0, limit7dResetsAt = 0;
    bool limit5hPresent = false, limit7dPresent = false;
    bool limitOpusPresent = false, limitSonnetPresent = false;

    bool extraEnabled = false;
    float extraUsedUsd = 0, extraLimitUsd = 0;

    GpuSnapshot gpu;
    char ollamaModel[64] = "";
    bool ollamaDataValid = false, ollamaError = false;

    bool dataValid = false, apiError = false;
    char lastError[64] = "";
    char wifiIP[16] = "";
    int16_t rssi = 0;
    uint32_t uptimeMs = 0, lastFetchMs = 0;

    struct tm timeinfo = {};
    bool timeValid = false;
};

// Loads /layout.json from LittleFS, falling back to the built-in default
// (mirroring the original 6 hardcoded screens) if missing or invalid.
void begin();

// Re-parses and replaces the in-memory layout from a JSON string (as posted
// to POST /api/layout). Validates widget bounds/count before accepting.
// Returns false (and leaves the current in-memory layout untouched) on
// invalid input.
bool applyJson(const String& json);

// Persists the current in-memory layout to /layout.json.
bool save();

// Serializes the current in-memory layout back to a JSON string.
String toJson();

// Number of enabled pages, in on-screen order.
uint8_t enabledPageCount();

// Returns the enabled page at position `idx` (0-based, in on-screen order).
const Page& enabledPageAt(uint8_t idx);

// Finds the on-screen index of the enabled page with the given id, or -1.
int enabledPageIndexById(const String& id);

// Draws all widgets for the given page into the content area
// (y in [TOPBAR_H, 135 - BOTBAR_H)). Caller is responsible for chrome
// (top bar / bottom bar / divider).
void renderPage(TFT_eSPI* dp, const Page& page, const LayoutContext& ctx);

} // namespace layoutEngine
