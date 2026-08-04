#include "layout_engine.h"
#include "ui_helpers.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

namespace layoutEngine {

static const char* LAYOUT_PATH = "/layout.json";
static const uint8_t MAX_PAGES = 12;
static const uint8_t MAX_WIDGETS_PER_PAGE = 16;

static std::vector<Page> g_pages;

// ============================================================
// Bind resolution
// ============================================================
static bool getPercentBind(const LayoutContext& ctx, const String& bind,
                            float& pct, bool& present, uint32_t& resetsAt, bool& hasCountdown) {
    present = true;
    hasCountdown = false;
    resetsAt = 0;
    if (bind == "limit_5h") {
        pct = ctx.limit5hPct; present = ctx.limit5hPresent;
        resetsAt = ctx.limit5hResetsAt; hasCountdown = true;
        return true;
    }
    if (bind == "limit_7d") {
        pct = ctx.limit7dPct; present = ctx.limit7dPresent;
        resetsAt = ctx.limit7dResetsAt; hasCountdown = true;
        return true;
    }
    if (bind == "limit_opus")   { pct = ctx.limitOpusPct;   present = ctx.limitOpusPresent;   return true; }
    if (bind == "limit_sonnet") { pct = ctx.limitSonnetPct; present = ctx.limitSonnetPresent;  return true; }
    if (bind == "extra_pct") {
        present = ctx.extraEnabled && ctx.extraLimitUsd > 0;
        pct = present ? (ctx.extraUsedUsd / ctx.extraLimitUsd) * 100.0f : 0;
        return true;
    }
    if (bind == "gpu_util") { pct = ctx.gpu.utilization; present = ctx.ollamaDataValid; return true; }
    if (bind == "gpu_vram") {
        present = ctx.ollamaDataValid && ctx.gpu.memTotal > 0;
        pct = present ? (ctx.gpu.memUsed / ctx.gpu.memTotal) * 100.0f : 0;
        return true;
    }
    return false;
}

static String getTextBind(const LayoutContext& ctx, const String& bind) {
    char buf[64];
    if (bind == "limit_opus_label") {
        snprintf(buf, sizeof(buf), "%.0f%%", ctx.limitOpusPct);
        return String(buf);
    }
    if (bind == "limit_sonnet_label") {
        snprintf(buf, sizeof(buf), "%.0f%%", ctx.limitSonnetPct);
        return String(buf);
    }
    if (bind == "extra_spend_label") {
        if (!ctx.extraEnabled) return "Extra spend off";
        if (ctx.extraLimitUsd > 0) {
            snprintf(buf, sizeof(buf), "$%.2f / $%.2f", ctx.extraUsedUsd, ctx.extraLimitUsd);
        } else {
            snprintf(buf, sizeof(buf), "$%.2f (unlimited)", ctx.extraUsedUsd);
        }
        return String(buf);
    }
    if (bind == "ollama_model") {
        if (ctx.ollamaError) return "Shuli not found";
        if (!ctx.ollamaDataValid) return "Fetching...";
        return String(ctx.ollamaModel);
    }
    if (bind == "ollama_model_clock_label") {
        snprintf(buf, sizeof(buf), "Shuli %s", ctx.ollamaModel);
        return String(buf);
    }
    if (bind == "gpu_temp") {
        snprintf(buf, sizeof(buf), "%.0f C", ctx.gpu.temp);
        return String(buf);
    }
    if (bind == "gpu_vram_label") {
        if (ctx.gpu.memTotal > 0) snprintf(buf, sizeof(buf), "%.0f / %.0f MiB", ctx.gpu.memUsed, ctx.gpu.memTotal);
        else snprintf(buf, sizeof(buf), "N/A");
        return String(buf);
    }
    if (bind == "data_status") {
        if (ctx.dataValid) return "Data: OK";
        if (ctx.apiError) return String(ctx.lastError);
        return "Waiting...";
    }
    if (bind == "wifi_rssi") {
        snprintf(buf, sizeof(buf), "RSSI: %d dBm", ctx.rssi);
        return String(buf);
    }
    if (bind == "uptime") {
        snprintf(buf, sizeof(buf), "Uptime: %lu min", (unsigned long)(ctx.uptimeMs / 60000));
        return String(buf);
    }
    if (bind == "fetch_age") {
        if (ctx.lastFetchMs == 0) return "";
        snprintf(buf, sizeof(buf), "Fetch: %lu s ago", (unsigned long)(ctx.lastFetchMs / 1000));
        return String(buf);
    }
    if (bind == "wifi_ip") {
        snprintf(buf, sizeof(buf), "IP: %s", ctx.wifiIP);
        return String(buf);
    }
    return "";
}

// ============================================================
// Rendering
// ============================================================
static void renderWidget(TFT_eSPI* dp, const Widget& w, const LayoutContext& ctx) {
    uint16_t color = uiColorFromName(w.color.c_str(), C_WHITE);

    switch (w.type) {
        case WIDGET_DONUT_GAUGE: {
            float pct = 0; bool present = true; uint32_t resetsAt = 0; bool hasCountdown = false;
            getPercentBind(ctx, w.bind, pct, present, resetsAt, hasCountdown);
            if (!present) break;
            uint16_t gaugeColor = color;
            float frac = pct / 100.0f;
            if (frac > 0.8f) gaugeColor = C_TOMATO;
            else if (frac > 0.5f) gaugeColor = C_AMBER;
            if (hasCountdown) uiDrawCountdown(dp, resetsAt, w.x, w.y - w.r - 24);
            uiDrawDonut(dp, w.x, w.y, w.r, w.thickness, frac, gaugeColor);
            uiDrawPctInside(dp, w.x, w.y, pct, C_WHITE);
            if (w.label.length()) {
                dp->setTextDatum(TC_DATUM);
                dp->setTextFont(1);
                dp->setTextColor(C_DIM, C_BG);
                dp->drawString(w.label.c_str(), w.x, w.y + w.r + 8);
            }
            break;
        }
        case WIDGET_BAR: {
            float pct = 0; bool present = true; uint32_t resetsAt = 0; bool hasCountdown = false;
            getPercentBind(ctx, w.bind, pct, present, resetsAt, hasCountdown);
            if (!present) break;
            uiDrawHBar(dp, w.x, w.y, w.w, w.h, pct);
            break;
        }
        case WIDGET_TEXT: {
            String text = w.bind.length() ? getTextBind(ctx, w.bind) : w.label;
            if (!text.length()) break;
            dp->setTextDatum(TL_DATUM);
            dp->setTextFont(w.font);
            dp->setTextColor(color, C_BG);
            dp->drawString(text.c_str(), w.x, w.y);
            break;
        }
        case WIDGET_CLOCK7SEG: {
            if (!ctx.timeValid) {
                dp->setTextDatum(MC_DATUM);
                dp->setTextFont(2);
                dp->setTextColor(C_DIM, C_BG);
                dp->drawString("Waiting for time...", 120, 68);
                break;
            }
            int16_t x = w.x, y = w.y;
            uiDraw7SegDigit(dp, x, y, ctx.timeinfo.tm_hour / 10, color); x += 30;
            uiDraw7SegDigit(dp, x, y, ctx.timeinfo.tm_hour % 10, color); x += 30;
            dp->fillRect(x + 4, y + 14, 5, 5, color);
            dp->fillRect(x + 4, y + 30, 5, 5, color);
            x += 16;
            uiDraw7SegDigit(dp, x, y, ctx.timeinfo.tm_min / 10, color); x += 30;
            uiDraw7SegDigit(dp, x, y, ctx.timeinfo.tm_min % 10, color); x += 30;
            dp->fillRect(x + 4, y + 14, 5, 5, color);
            dp->fillRect(x + 4, y + 30, 5, 5, color);
            x += 16;
            uiDraw7SegDigit(dp, x, y, ctx.timeinfo.tm_sec / 10, color); x += 30;
            uiDraw7SegDigit(dp, x, y, ctx.timeinfo.tm_sec % 10, color);
            break;
        }
        default: break;
    }
}

void renderPage(TFT_eSPI* dp, const Page& page, const LayoutContext& ctx) {
    for (const Widget& w : page.widgets) renderWidget(dp, w, ctx);
}

// ============================================================
// Default layout (mirrors the original 6 hardcoded screens)
// ============================================================
static void buildDefaultLayout() {
    g_pages.clear();

    {
        Page p; p.id = "main"; p.enabled = true;
        Widget g1; g1.type = WIDGET_DONUT_GAUGE; g1.x = 60; g1.y = 74; g1.r = 34; g1.thickness = 7;
        g1.bind = "limit_5h"; g1.color = "amber"; g1.label = "5-HOUR";
        Widget g2; g2.type = WIDGET_DONUT_GAUGE; g2.x = 180; g2.y = 74; g2.r = 34; g2.thickness = 7;
        g2.bind = "limit_7d"; g2.color = "lavender"; g2.label = "7-DAY";
        p.widgets = { g1, g2 };
        g_pages.push_back(p);
    }
    {
        Page p; p.id = "opus_sonnet"; p.enabled = true;
        Widget l1; l1.type = WIDGET_TEXT; l1.x = 8; l1.y = 22; l1.font = 2; l1.color = "dim"; l1.label = "Opus";
        Widget v1; v1.type = WIDGET_TEXT; v1.x = 8; v1.y = 42; v1.font = 2; v1.color = "white"; v1.bind = "limit_opus_label";
        Widget b1; b1.type = WIDGET_BAR; b1.x = 100; b1.y = 42; b1.w = 110; b1.h = 12; b1.bind = "limit_opus";
        Widget l2; l2.type = WIDGET_TEXT; l2.x = 8; l2.y = 66; l2.font = 2; l2.color = "dim"; l2.label = "Sonnet";
        Widget v2; v2.type = WIDGET_TEXT; v2.x = 8; v2.y = 86; v2.font = 2; v2.color = "white"; v2.bind = "limit_sonnet_label";
        Widget b2; b2.type = WIDGET_BAR; b2.x = 100; b2.y = 86; b2.w = 110; b2.h = 12; b2.bind = "limit_sonnet";
        p.widgets = { l1, v1, b1, l2, v2, b2 };
        g_pages.push_back(p);
    }
    {
        Page p; p.id = "extra"; p.enabled = true;
        Widget l1; l1.type = WIDGET_TEXT; l1.x = 8; l1.y = 22; l1.font = 2; l1.color = "dim"; l1.label = "Extra Spend";
        Widget v1; v1.type = WIDGET_TEXT; v1.x = 8; v1.y = 44; v1.font = 2; v1.color = "white"; v1.bind = "extra_spend_label";
        Widget b1; b1.type = WIDGET_BAR; b1.x = 20; b1.y = 68; b1.w = 200; b1.h = 14; b1.bind = "extra_pct";
        p.widgets = { l1, v1, b1 };
        g_pages.push_back(p);
    }
    {
        Page p; p.id = "ollama"; p.enabled = true;
        Widget l1; l1.type = WIDGET_TEXT; l1.x = 8; l1.y = 18; l1.font = 1; l1.color = "dim"; l1.label = "MODEL:";
        Widget v1; v1.type = WIDGET_TEXT; v1.x = 60; v1.y = 18; v1.font = 1; v1.color = "ice"; v1.bind = "ollama_model";
        Widget l2; l2.type = WIDGET_TEXT; l2.x = 8; l2.y = 38; l2.font = 1; l2.color = "dim"; l2.label = "GPU:";
        Widget b2; b2.type = WIDGET_BAR; b2.x = 60; b2.y = 38; b2.w = 120; b2.h = 10; b2.bind = "gpu_util";
        Widget l3; l3.type = WIDGET_TEXT; l3.x = 8; l3.y = 54; l3.font = 1; l3.color = "dim"; l3.label = "VRAM:";
        Widget v3; v3.type = WIDGET_TEXT; v3.x = 60; v3.y = 54; v3.font = 1; v3.color = "white"; v3.bind = "gpu_vram_label";
        Widget l4; l4.type = WIDGET_TEXT; l4.x = 8; l4.y = 74; l4.font = 1; l4.color = "dim"; l4.label = "TEMP:";
        Widget v4; v4.type = WIDGET_TEXT; v4.x = 60; v4.y = 74; v4.font = 1; v4.color = "white"; v4.bind = "gpu_temp";
        p.widgets = { l1, v1, l2, b2, l3, v3, l4, v4 };
        g_pages.push_back(p);
    }
    {
        Page p; p.id = "clock"; p.enabled = true;
        Widget c1; c1.type = WIDGET_CLOCK7SEG; c1.x = 22; c1.y = 26; c1.color = "tomato";
        Widget v1; v1.type = WIDGET_TEXT; v1.x = 120; v1.y = 88; v1.font = 1; v1.color = "dim"; v1.bind = "ollama_model_clock_label";
        p.widgets = { c1, v1 };
        g_pages.push_back(p);
    }
    {
        Page p; p.id = "status"; p.enabled = true;
        Widget v1; v1.type = WIDGET_TEXT; v1.x = 8; v1.y = 18; v1.font = 1; v1.color = "green"; v1.bind = "data_status";
        Widget v2; v2.type = WIDGET_TEXT; v2.x = 8; v2.y = 30; v2.font = 1; v2.color = "white"; v2.bind = "wifi_rssi";
        Widget v3; v3.type = WIDGET_TEXT; v3.x = 8; v3.y = 42; v3.font = 1; v3.color = "white"; v3.bind = "uptime";
        Widget v4; v4.type = WIDGET_TEXT; v4.x = 8; v4.y = 54; v4.font = 1; v4.color = "white"; v4.bind = "fetch_age";
        Widget v5; v5.type = WIDGET_TEXT; v5.x = 8; v5.y = 66; v5.font = 1; v5.color = "white"; v5.bind = "wifi_ip";
        p.widgets = { v1, v2, v3, v4, v5 };
        g_pages.push_back(p);
    }
}

// ============================================================
// JSON (de)serialization
// ============================================================
static WidgetType widgetTypeFromString(const String& s) {
    if (s == "donutGauge") return WIDGET_DONUT_GAUGE;
    if (s == "bar") return WIDGET_BAR;
    if (s == "text") return WIDGET_TEXT;
    if (s == "clock7seg") return WIDGET_CLOCK7SEG;
    return WIDGET_UNKNOWN;
}

static const char* widgetTypeToString(WidgetType t) {
    switch (t) {
        case WIDGET_DONUT_GAUGE: return "donutGauge";
        case WIDGET_BAR: return "bar";
        case WIDGET_TEXT: return "text";
        case WIDGET_CLOCK7SEG: return "clock7seg";
        default: return "unknown";
    }
}

// Bounds-checks widget geometry against the 240x135 panel and caps
// pages/widgets so a malformed POST /api/layout can't corrupt storage or
// blow the heap.
static bool parseLayoutFromDoc(JsonDocument& doc, std::vector<Page>& out) {
    JsonArray pagesArr = doc["pages"].as<JsonArray>();
    if (pagesArr.isNull() || pagesArr.size() == 0 || pagesArr.size() > MAX_PAGES) return false;

    std::vector<Page> parsed;
    for (JsonObject pObj : pagesArr) {
        Page p;
        p.id = pObj["id"] | "page";
        p.enabled = pObj["enabled"] | true;

        JsonArray widgetsArr = pObj["widgets"].as<JsonArray>();
        if (!widgetsArr.isNull()) {
            if (widgetsArr.size() > MAX_WIDGETS_PER_PAGE) return false;
            for (JsonObject wObj : widgetsArr) {
                Widget w;
                w.type = widgetTypeFromString(wObj["type"] | "");
                if (w.type == WIDGET_UNKNOWN) continue;
                w.x = wObj["x"] | 0;
                w.y = wObj["y"] | 0;
                w.w = wObj["w"] | 0;
                w.h = wObj["h"] | 0;
                w.r = wObj["r"] | 34;
                w.thickness = wObj["thickness"] | 7;
                w.bind = String((const char*)(wObj["bind"] | ""));
                w.color = String((const char*)(wObj["color"] | "white"));
                w.label = String((const char*)(wObj["label"] | ""));
                w.font = wObj["font"] | 2;

                if (w.x < 0 || w.x > 240 || w.y < 0 || w.y > 135) continue;
                if (w.w < 0 || w.w > 240 || w.h < 0 || w.h > 135) continue;
                if (w.r < 0 || w.r > 120) continue;

                p.widgets.push_back(w);
            }
        }
        parsed.push_back(p);
    }
    if (parsed.empty()) return false;
    out = parsed;
    return true;
}

bool applyJson(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    std::vector<Page> parsed;
    if (!parseLayoutFromDoc(doc, parsed)) return false;
    g_pages = parsed;
    return true;
}

String toJson() {
    JsonDocument doc;
    JsonArray pagesArr = doc["pages"].to<JsonArray>();
    for (const Page& p : g_pages) {
        JsonObject pObj = pagesArr.add<JsonObject>();
        pObj["id"] = p.id;
        pObj["enabled"] = p.enabled;
        JsonArray widgetsArr = pObj["widgets"].to<JsonArray>();
        for (const Widget& w : p.widgets) {
            JsonObject wObj = widgetsArr.add<JsonObject>();
            wObj["type"] = widgetTypeToString(w.type);
            wObj["x"] = w.x;
            wObj["y"] = w.y;
            if (w.w) wObj["w"] = w.w;
            if (w.h) wObj["h"] = w.h;
            if (w.type == WIDGET_DONUT_GAUGE) {
                wObj["r"] = w.r;
                wObj["thickness"] = w.thickness;
            }
            if (w.bind.length()) wObj["bind"] = w.bind;
            if (w.color.length()) wObj["color"] = w.color;
            if (w.label.length()) wObj["label"] = w.label;
            if (w.type == WIDGET_TEXT) wObj["font"] = w.font;
        }
    }
    String out;
    serializeJson(doc, out);
    return out;
}

bool save() {
    fs::File f = LittleFS.open(LAYOUT_PATH, "w");
    if (!f) return false;
    String json = toJson();
    size_t written = f.print(json);
    f.close();
    return written == json.length();
}

void begin() {
    buildDefaultLayout();
    if (LittleFS.exists(LAYOUT_PATH)) {
        fs::File f = LittleFS.open(LAYOUT_PATH, "r");
        if (f) {
            String json = f.readString();
            f.close();
            if (!applyJson(json)) {
                Serial.println("layoutEngine: /layout.json invalid, using built-in default");
                buildDefaultLayout();
            }
        }
    }
}

uint8_t enabledPageCount() {
    uint8_t n = 0;
    for (const Page& p : g_pages) if (p.enabled) n++;
    return n;
}

const Page& enabledPageAt(uint8_t idx) {
    uint8_t n = 0;
    for (const Page& p : g_pages) {
        if (!p.enabled) continue;
        if (n == idx) return p;
        n++;
    }
    return g_pages.front();
}

int enabledPageIndexById(const String& id) {
    int n = 0;
    for (const Page& p : g_pages) {
        if (!p.enabled) continue;
        if (p.id == id) return n;
        n++;
    }
    return -1;
}

} // namespace layoutEngine
