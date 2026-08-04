#include <Arduino.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <LittleFS.h>
#include <time.h>

#include "layout_engine.h"
#include "button_actions.h"
#include "web_server.h"

#define BTN_1         35
#define BTN_2          0
#define WIFI_TIMEOUT  15000
#define REFRESH_MS   300000
#define OLLAMA_REFRESH_MS 10000
#define SHULI_HOST    "192.168.1.118"
#define SHULI_OLLAMA  "http://" SHULI_HOST ":11434"
#define SHULI_GPU     "http://" SHULI_HOST ":8765"

TFT_eSPI tft;
static TFT_eSPI* dp = &tft;
Preferences prefs;
WebServer webServer(80);

static String proxyUrl;
static String sessionKey;
static String orgUuid;
static String otaPassword;

struct LimitData {
    float utilization;
    uint32_t resetsAt;
    bool present;
};

static LimitData limit5h, limit7d, limitOpus, limitSonnet;
static bool extraEnabled = false;
static uint32_t extraUsed = 0, extraLimit = 0;

struct GpuInfo {
    float utilization;
    float memUsed;
    float memTotal;
    float temp;
    char name[24];
};
static GpuInfo gpuInfo = {};
static char ollamaModel[64] = "";
static bool ollamaDataValid = false;
static bool ollamaError = false;
static char ollamaLastError[64] = "";
static uint32_t lastOllamaFetch = 0;

static bool dataValid = false;
static bool apiError = false;
static char lastError[64] = "";
static char wifiIP[16] = "";
static bool wifiConnected = false;
static bool needsConfig = true;
static bool isFetching = false;
static uint32_t lastFetch = 0;
static uint32_t uptimeStart = 0;
static uint32_t nextRefresh = 0;

// ============================================================
// LCARS Color Palette (RGB565)
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
// Layout (240 × 135 landscape)
// ============================================================
#define TOPBAR_H  14
#define BOTBAR_H  13
#define CONTENT_Y TOPBAR_H
#define CONTENT_H (135 - TOPBAR_H - BOTBAR_H)

// Left gauge (5h)
#define L5_CD_X   60
#define L5_CD_Y   (TOPBAR_H + 2)
#define L5_G_X    60
#define L5_G_Y    (TOPBAR_H + 60)
#define L5_G_R    34
#define L5_G_T     7

// Divider
#define DIV_X     119
#define DIV_Y     (TOPBAR_H + 2)
#define DIV_H     (CONTENT_H - 4)

// Right gauge (7d)
#define R7_CD_X   180
#define R7_CD_Y   L5_CD_Y
#define R7_G_X    180
#define R7_G_Y    L5_G_Y
#define R7_G_R    L5_G_R
#define R7_G_T    L5_G_T

// Bottom bar sub-elements
#define PAGE_X    4
#define PAGE_Y    (134 - BOTBAR_H + 2)
#define PAGE_W    26
#define PAGE_H    (BOTBAR_H - 4)
#define SIG_X     68
#define SIG_Y     (134 - BOTBAR_H + 1)
#define REFRESH_X 116
#define REFRESH_W 120
#define REFRESH_Y PAGE_Y
#define REFRESH_H PAGE_H

// ============================================================
// Drawing Helpers
// ============================================================

static void drawTopBar() {
    dp->fillRect(0, 0, 240, TOPBAR_H, C_TOPBAR);
    dp->setTextDatum(TR_DATUM);
    dp->setTextFont(2);
    dp->setTextColor(C_PEACH, C_TOPBAR);
    dp->drawString("CLAUDE.AI", 236, 2);
    dp->drawFastHLine(0, TOPBAR_H - 1, 240, C_SEPARATOR);
}

static void drawBottomBar(uint8_t screenIdx, uint8_t screenCount) {
    dp->fillRect(0, 135 - BOTBAR_H, 240, BOTBAR_H, C_BOTBAR);
    dp->drawFastHLine(0, 135 - BOTBAR_H, 240, C_SEPARATOR);
    int16_t by = 135 - BOTBAR_H + 1;

    // Page chip
    dp->fillSmoothRoundRect(PAGE_X, PAGE_Y, PAGE_W, PAGE_H, PAGE_H / 2,
                             C_PEACH, C_BOTBAR);
    dp->fillRect(PAGE_X, PAGE_Y, PAGE_H / 2, PAGE_H, C_PEACH);
    char pageBuf[8];
    snprintf(pageBuf, sizeof(pageBuf), "%d/%d", screenIdx + 1, screenCount);
    dp->setTextDatum(MC_DATUM);
    dp->setTextFont(1);
    dp->setTextColor(C_BG, C_PEACH);
    dp->drawString(pageBuf, PAGE_X + PAGE_W / 2, PAGE_Y + PAGE_H / 2);

    // Signal bars
    int16_t rssi = WiFi.RSSI();
    int bars = 0;
    if (wifiConnected) {
        if (rssi > -55) bars = 4;
        else if (rssi > -65) bars = 3;
        else if (rssi > -75) bars = 2;
        else if (rssi > -85) bars = 1;
    }
    int16_t barHeights[] = { 3, 6, 9, 11 };
    int16_t baseY = SIG_Y + 11;
    for (int i = 0; i < 4; i++) {
        uint16_t c = (i < bars) ? C_ICE : C_DIM;
        dp->fillRect(SIG_X + i * 4, baseY - barHeights[i], 3, barHeights[i], c);
    }

    dp->setTextDatum(ML_DATUM);
    dp->setTextFont(1);
    dp->setTextColor(C_DIM, C_BOTBAR);
    dp->drawString("SIGNAL", SIG_X + 20, SIG_Y + 5);

    // Refresh pill
    dp->fillSmoothRoundRect(REFRESH_X, REFRESH_Y, REFRESH_W, REFRESH_H,
                             REFRESH_H / 2, C_AMBER, C_BOTBAR);
    dp->fillRect(REFRESH_X, REFRESH_Y, REFRESH_H / 2, REFRESH_H, C_AMBER);
    dp->setTextDatum(ML_DATUM);
    dp->setTextFont(1);
    dp->setTextColor(C_BG, C_AMBER);
    dp->drawString("REFRESH IN", REFRESH_X + 3, REFRESH_Y + REFRESH_H / 2);

    // Segmented progress in refresh pill
    int16_t barX = REFRESH_X + 62;
    int16_t barY = REFRESH_Y + 2;
    int16_t barH = REFRESH_H - 4;
    int16_t barEnd = REFRESH_X + REFRESH_W - REFRESH_H / 2 - 2;
    float pct = 0.0f;
    if (nextRefresh > lastFetch) {
        uint32_t elapsed = millis() - lastFetch;
        uint32_t total = nextRefresh - lastFetch;
        if (total > 0) pct = (float)elapsed / (float)total;
        if (pct > 1.0f) pct = 1.0f;
    }
    int nSegs = 0;
    for (int16_t sx = barX; sx + 3 <= barEnd; sx += 4) nSegs++;
    int filled = (int)(nSegs * pct + 0.5f);
    int16_t sx = barX;
    for (int seg = 0; sx + 3 <= barEnd; seg++) {
        uint16_t c = (seg < filled) ? C_BG : 0x0000;
        dp->fillRect(sx, barY, 3, barH, c);
        sx += 4;
    }
}

// ============================================================
// 7-Segment Clock
// ============================================================
static void draw7SegDigit(int16_t x, int16_t y, uint8_t d, uint16_t color) {
    static const uint8_t seg[10] = {
        0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
        0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111
    };
    uint8_t m = seg[d % 10];
    int16_t W = 26, H = 42, T = 4;
    // a (top)
    if (m & 1) dp->fillRect(x + T, y, W - T*2, T, color);
    // b (top-right)
    if (m & 2) dp->fillRect(x + W - T, y + T, T, H/2 - T, color);
    // c (bottom-right)
    if (m & 4) dp->fillRect(x + W - T, y + H/2, T, H/2 - T, color);
    // d (bottom)
    if (m & 8) dp->fillRect(x + T, y + H - T, W - T*2, T, color);
    // e (bottom-left)
    if (m & 16) dp->fillRect(x, y + H/2, T, H/2 - T, color);
    // f (top-left)
    if (m & 32) dp->fillRect(x, y + T, T, H/2 - T, color);
    // g (middle)
    if (m & 64) dp->fillRect(x + T, y + H/2 - T/2, W - T*2, T, color);
}

static uint32_t lastClockSec = 0;

static void drawBootClock() {
    time_t now;
    time(&now);
    struct tm* ti = localtime(&now);
    if (!ti) {
        dp->setTextDatum(MC_DATUM);
        dp->setTextFont(2);
        dp->setTextColor(C_DIM, C_BG);
        dp->drawString("Waiting for time...", 120, 88);
        return;
    }
    uint8_t hh = ti->tm_hour, mm = ti->tm_min, ss = ti->tm_sec;
    uint32_t sec = hh*3600 + mm*60 + ss;
    if (sec == lastClockSec) return;
    lastClockSec = sec;

    dp->fillRect(0, 30, 240, 75, C_BG);

    int16_t x = 22;
    int16_t y = 44;
    uint16_t clr = 0xF800;

    draw7SegDigit(x, y, hh / 10, clr); x += 28;
    draw7SegDigit(x, y, hh % 10, clr); x += 28;
    // colon
    dp->fillRect(x + 4, y + 12, 4, 4, clr);
    dp->fillRect(x + 4, y + 28, 4, 4, clr);
    x += 14;
    draw7SegDigit(x, y, mm / 10, clr); x += 28;
    draw7SegDigit(x, y, mm % 10, clr); x += 28;
    // colon
    dp->fillRect(x + 4, y + 12, 4, 4, clr);
    dp->fillRect(x + 4, y + 28, 4, 4, clr);
    x += 14;
    draw7SegDigit(x, y, ss / 10, clr); x += 28;
    draw7SegDigit(x, y, ss % 10, clr);
}

static void drawLoading() {
    dp->fillRect(0, TOPBAR_H, 240, CONTENT_H, C_BG);
    dp->setTextDatum(MC_DATUM);
    dp->setTextFont(2);
    dp->setTextColor(C_AMBER, C_BG);
    dp->drawString(isFetching ? "Fetching..." : "Starting...", 120, 68);
}

static void drawError() {
    dp->fillRect(0, TOPBAR_H, 240, CONTENT_H, C_BG);
    dp->setTextDatum(MC_DATUM);
    dp->setTextFont(2);
    dp->setTextColor(C_TOMATO, C_BG);
    dp->drawString(lastError, 120, 68);
}

// ============================================================
// Screens
// ============================================================

// ============================================================
// Dispatch — pages/widgets are data-driven, see layout_engine.{h,cpp}.
// ============================================================
static uint8_t currentPageIndex = 0;

static layoutEngine::LayoutContext buildLayoutContext() {
    layoutEngine::LayoutContext ctx;
    ctx.limit5hPct = limit5h.utilization;
    ctx.limit5hResetsAt = limit5h.resetsAt;
    ctx.limit5hPresent = limit5h.present;
    ctx.limit7dPct = limit7d.utilization;
    ctx.limit7dResetsAt = limit7d.resetsAt;
    ctx.limit7dPresent = limit7d.present;
    ctx.limitOpusPct = limitOpus.utilization;
    ctx.limitOpusPresent = limitOpus.present;
    ctx.limitSonnetPct = limitSonnet.utilization;
    ctx.limitSonnetPresent = limitSonnet.present;

    ctx.extraEnabled = extraEnabled;
    ctx.extraUsedUsd = extraUsed / 100.0f;
    ctx.extraLimitUsd = extraLimit / 100.0f;

    ctx.gpu.utilization = gpuInfo.utilization;
    ctx.gpu.memUsed = gpuInfo.memUsed;
    ctx.gpu.memTotal = gpuInfo.memTotal;
    ctx.gpu.temp = gpuInfo.temp;
    strncpy(ctx.gpu.name, gpuInfo.name, sizeof(ctx.gpu.name));
    strncpy(ctx.ollamaModel, ollamaModel, sizeof(ctx.ollamaModel));
    ctx.ollamaDataValid = ollamaDataValid;
    ctx.ollamaError = ollamaError;

    ctx.dataValid = dataValid;
    ctx.apiError = apiError;
    strncpy(ctx.lastError, lastError, sizeof(ctx.lastError));
    strncpy(ctx.wifiIP, wifiIP, sizeof(ctx.wifiIP));
    ctx.rssi = WiFi.RSSI();
    ctx.uptimeMs = millis() - uptimeStart;
    ctx.lastFetchMs = lastFetch > 0 ? (millis() - lastFetch) : 0;

    time_t now;
    time(&now);
    struct tm* ti = localtime(&now);
    if (ti) { ctx.timeinfo = *ti; ctx.timeValid = true; }

    return ctx;
}

static bool backlightOn = true;

static void drawScreen() {
    // Double-buffer via sprite to eliminate flicker
    TFT_eSprite spr(&tft);
    spr.setColorDepth(16);
    bool useSprite = spr.createSprite(240, 135);
    if (useSprite) {
        dp = &spr;
        spr.fillSprite(C_BG);
    }

    if (sessionKey.isEmpty()) {
        drawTopBar();
        dp->fillRect(0, TOPBAR_H, 240, CONTENT_H, C_BG);
        dp->setTextDatum(MC_DATUM);
        dp->setTextFont(2);
        dp->setTextColor(C_AMBER, C_BG);
        dp->drawString("No session key", 120, 50);
        dp->setTextFont(1);
        dp->setTextColor(C_DIM, C_BG);
        char buf[32];
        snprintf(buf, sizeof(buf), "http://%s", wifiIP);
        dp->drawString("Open browser to:", 120, 68);
        dp->setTextColor(C_ICE, C_BG);
        dp->drawString(buf, 120, 80);
        drawBottomBar(0, 1);
    } else if (isFetching && !dataValid) {
        dp->fillRect(0, TOPBAR_H, 240, CONTENT_H, C_BG);
        dp->setTextDatum(MC_DATUM);
        dp->setTextFont(2);
        dp->setTextColor(C_AMBER, C_BG);
        dp->drawString("Restarting...", 120, 14);
        dp->setTextFont(1);
        dp->setTextColor(C_DIM, C_BG);
        dp->drawString("Fetching data...", 120, 120);
        drawBootClock();
    } else if (apiError && !dataValid) {
        drawTopBar();
        drawError();
        drawBottomBar(currentPageIndex, layoutEngine::enabledPageCount());
    } else {
        uint8_t total = layoutEngine::enabledPageCount();
        if (currentPageIndex >= total) currentPageIndex = 0;
        drawTopBar();
        dp->fillRect(0, TOPBAR_H, 240, CONTENT_H, C_BG);
        layoutEngine::LayoutContext ctx = buildLayoutContext();
        layoutEngine::renderPage(dp, layoutEngine::enabledPageAt(currentPageIndex), ctx);
        drawBottomBar(currentPageIndex, total);
    }

    if (useSprite) {
        spr.pushSprite(0, 0);
        spr.deleteSprite();
        dp = &tft;
    }
}

// ============================================================
// Claude.ai subscription API via proxy
// ============================================================
static bool proxyGet(const String& path, String& response) {
    String fullUrl = proxyUrl + path;
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(10000);
    http.setReuse(false);

    if (!http.begin(fullUrl)) {
        snprintf(lastError, sizeof(lastError), "HTTP begin fail");
        return false;
    }
    http.addHeader("X-Session-Key", sessionKey);

    int code = http.GET();
    if (code <= 0) {
        snprintf(lastError, sizeof(lastError), "Proxy unreachable");
        http.end();
        return false;
    }
    if (code != 200) {
        snprintf(lastError, sizeof(lastError), "Proxy HTTP %d", code);
        http.end();
        return false;
    }
    response = http.getString();
    http.end();
    return true;
}

static bool shuliGet(const String& fullUrl, String& response) {
    HTTPClient http;
    http.setConnectTimeout(4000);
    http.setTimeout(5000);
    http.setReuse(false);
    if (!http.begin(fullUrl)) return false;
    int code = http.GET();
    if (code != 200) { http.end(); return false; }
    response = http.getString();
    http.end();
    return true;
}

void fetchOllamaAndGpu() {
    if (!wifiConnected) return;
    ollamaDataValid = false;
    ollamaError = false;

    // GPU
    {
        String resp;
        if (shuliGet(String(SHULI_GPU) + "/gpu", resp)) {
            JsonDocument doc;
            if (!deserializeJson(doc, resp)) {
                JsonArray arr = doc.as<JsonArray>();
                if (arr.size() > 0) {
                    JsonObject g = arr[0];
                    gpuInfo.utilization = g["utilization"] | 0.0f;
                    gpuInfo.memUsed = g["mem_used_mib"] | 0.0f;
                    gpuInfo.memTotal = g["mem_total_mib"] | 0.0f;
                    gpuInfo.temp = g["temp_c"] | 0.0f;
                    strncpy(gpuInfo.name, g["name"] | "?", sizeof(gpuInfo.name));
                    ollamaDataValid = true;
                }
            }
        }
    }

    // Ollama
    {
        String resp;
        if (shuliGet(String(SHULI_OLLAMA) + "/api/ps", resp)) {
            JsonDocument doc;
            if (!deserializeJson(doc, resp)) {
                JsonArray models = doc["models"];
                if (models.size() > 0) {
                    strncpy(ollamaModel, models[0]["name"] | "?", sizeof(ollamaModel));
                    ollamaDataValid = true;
                } else {
                    strncpy(ollamaModel, "(idle)", sizeof(ollamaModel));
                    ollamaDataValid = true;
                }
            }
        } else {
            // Ollama unreachable — still valid if GPU worked
            if (!ollamaDataValid) {
                ollamaError = true;
                snprintf(ollamaLastError, sizeof(ollamaLastError), "Shuli unreachable");
            } else {
                strncpy(ollamaModel, "(no ps)", sizeof(ollamaModel));
            }
        }
    }

    lastOllamaFetch = millis();
}

static bool fetchOrgUuid() {
    String resp;
    if (!proxyGet("/api/organizations", resp)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, resp)) {
        snprintf(lastError, sizeof(lastError), "Org JSON parse fail");
        return false;
    }
    JsonArray arr = doc.as<JsonArray>();
    if (arr.size() == 0) {
        snprintf(lastError, sizeof(lastError), "No orgs found");
        return false;
    }
    const char* uuid = arr[0]["uuid"] | "";
    if (strlen(uuid) == 0) {
        snprintf(lastError, sizeof(lastError), "No org UUID");
        return false;
    }
    orgUuid = uuid;
    return true;
}

static void parseLimit(JsonVariant obj, LimitData& lim) {
    lim.present = !obj.isNull();
    if (!lim.present) return;
    lim.utilization = obj["utilization"] | 0.0f;
    const char* r = obj["resets_at"] | "";
    if (strlen(r)) {
        struct tm tm = {}; char buf[32];
        strncpy(buf, r, sizeof(buf) - 1);
        sscanf(buf, "%d-%d-%dT%d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        lim.resetsAt = mktime(&tm);
    }
}

void fetchData() {
    if (!wifiConnected) { Serial.println("fetchData: no wifi"); return; }
    if (sessionKey.isEmpty()) {
        Serial.println("fetchData: no session key");
        snprintf(lastError, sizeof(lastError), "No session key");
        apiError = true; isFetching = false;
        lastFetch = millis(); return;
    }
    Serial.println("fetchData: starting");
    isFetching = true;
    apiError = false;

    if (orgUuid.isEmpty()) {
        Serial.println("fetchData: fetching org UUID");
        if (!fetchOrgUuid()) {
            Serial.print("fetchData: org UUID failed: "); Serial.println(lastError);
            apiError = true; isFetching = false;
            lastFetch = millis(); return;
        }
        Serial.print("fetchData: org UUID = "); Serial.println(orgUuid);
    }

    {
        Serial.println("fetchData: fetching usage");
        String path = "/api/organizations/" + orgUuid + "/usage";
        String resp;
        if (!proxyGet(path, resp)) {
            Serial.print("fetchData: usage fetch failed: "); Serial.println(lastError);
            apiError = true; isFetching = false;
            lastFetch = millis(); return;
        }
        Serial.print("fetchData: usage response length="); Serial.println(resp.length());
        JsonDocument doc;
        if (deserializeJson(doc, resp)) {
            Serial.println("fetchData: usage JSON parse fail");
            snprintf(lastError, sizeof(lastError), "Usage JSON fail");
            apiError = true; isFetching = false;
            lastFetch = millis(); return;
        }
        Serial.println("fetchData: usage parsed OK");
        parseLimit(doc["five_hour"], limit5h);
        parseLimit(doc["seven_day"], limit7d);
        parseLimit(doc["seven_day_opus"], limitOpus);
        parseLimit(doc["seven_day_sonnet"], limitSonnet);
    }

    {
        Serial.println("fetchData: fetching extra spend");
        String path = "/api/organizations/" + orgUuid + "/overage_spend_limit";
        String resp;
        if (proxyGet(path, resp)) {
            JsonDocument doc;
            if (!deserializeJson(doc, resp)) {
                extraEnabled = doc["is_enabled"] | false;
                if (extraEnabled) {
                    extraUsed = doc["used_credits"] | 0;
                    if (!doc["monthly_credit_limit"].isNull())
                        extraLimit = doc["monthly_credit_limit"] | 0;
                }
                Serial.println("fetchData: extra spend parsed OK");
            }
        } else {
            Serial.println("fetchData: extra spend fetch failed (non-critical)");
        }
    }

    dataValid = true;
    apiError = false;
    isFetching = false;
    lastFetch = millis();
    nextRefresh = lastFetch + REFRESH_MS;
    Serial.println("fetchData: done, data valid");
}

// ============================================================
// WiFi / Config
// ============================================================
void setupWiFi() {
    String ssid = prefs.getString("wifi_ssid", "");
    String pass = prefs.getString("wifi_pass", "");
    if (ssid.isEmpty()) { needsConfig = true; return; }

    Serial.print("Connecting to WiFi: "); Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (wifiConnected) {
        strncpy(wifiIP, WiFi.localIP().toString().c_str(), sizeof(wifiIP));
        needsConfig = false;
        configTzTime("IST-2IDT,M3.5.5,M10.5.0", "pool.ntp.org");
        Serial.print("WiFi connected, IP: "); Serial.println(wifiIP);
    } else {
        Serial.print("WiFi failed, status: "); Serial.println(WiFi.status());
    }
}

// Web config UI, WiFi/proxy/session save, and OTA web-upload now live in
// mgmtServer (src/web_server.cpp), registered from startWebConfig() below.

// ============================================================
// OTA (over-the-air) updates
// ============================================================
void setupOTA() {
    ArduinoOTA.setHostname("claudegauge-td");
    if (!otaPassword.isEmpty()) ArduinoOTA.setPassword(otaPassword.c_str());

    ArduinoOTA.onStart([]() {
        dp->fillScreen(C_BG);
        dp->setTextDatum(MC_DATUM);
        dp->setTextFont(2);
        dp->setTextColor(C_AMBER, C_BG);
        dp->drawString("OTA update...", 120, 50);
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        int pct = total ? (int)((progress * 100UL) / total) : 0;
        dp->drawRect(20, 75, 200, 16, C_DIM);
        dp->fillRect(21, 76, (int)(198 * pct / 100.0f), 14, C_GREEN);
    });
    ArduinoOTA.onEnd([]() {
        dp->setTextDatum(MC_DATUM);
        dp->setTextFont(2);
        dp->setTextColor(C_GREEN, C_BG);
        dp->drawString("OTA done, rebooting", 120, 100);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        dp->setTextDatum(MC_DATUM);
        dp->setTextFont(2);
        dp->setTextColor(C_TOMATO, C_BG);
        dp->drawString("OTA error", 120, 100);
    });
    ArduinoOTA.begin();
    Serial.println("ArduinoOTA ready (hostname: claudegauge-td.local)");
}

void enterAPMode() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ClaudeGauge-TD");
}

void startWebConfig() {
    mgmtServer::begin(webServer, prefs, tft, otaPassword);
    webServer.begin();
    Serial.print("Web config at http://"); Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\nClaudeGauge TTGO T-Display starting...");

    dp->init();
    dp->setRotation(1);
    dp->fillScreen(C_BG);
    dp->setTextDatum(MC_DATUM);
    dp->setTextFont(2);
    dp->setTextColor(C_AMBER, C_BG);
    dp->drawString("Restarting...", 120, 68);

    prefs.begin("claugauge", false);
    Serial.println("NVS opened");

    String savedSSID = prefs.getString("wifi_ssid", "");
    if (savedSSID.isEmpty()) {
        prefs.putString("wifi_ssid", "bobsfog_2.4");
        prefs.putString("wifi_pass", "029466372");
        Serial.println("Set default WiFi creds in NVS");
    }

    sessionKey = prefs.getString("session_key", "");
    proxyUrl = prefs.getString("proxy_url", "https://cloud-proxy-three.vercel.app");
    orgUuid = prefs.getString("org_uuid", "");
    otaPassword = prefs.getString("ota_pass", "claudegauge");

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    }
    layoutEngine::begin();
    buttonActions::begin();

    pinMode(BTN_1, INPUT);
    pinMode(BTN_2, INPUT_PULLUP);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL, 0);
    ledcWrite(0, 255);
    uptimeStart = millis();

    setupWiFi();
    startWebConfig();
    if (needsConfig) {
        Serial.println("Entering AP mode");
        enterAPMode();
    } else {
        Serial.println("WiFi OK, normal operation");
        lastFetch = 0;
        setupOTA();
    }
}

void loop() {
    webServer.handleClient();

    if (needsConfig) {
        dp->fillScreen(C_BG);
        dp->setTextColor(C_WHITE, C_BG);
        dp->drawString("SETUP MODE", 4, 10, 2);
        dp->setTextColor(C_DIM, C_BG);
        dp->drawString("Connect to WiFi:", 4, 30, 1);
        dp->drawString("SSID: ClaudeGauge-TD", 4, 42, 1);
        dp->drawString("Open 192.168.4.1", 4, 54, 1);
        dp->drawString("in your browser", 4, 66, 1);
        delay(100);
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        unsigned long start = millis();
        WiFi.reconnect();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) delay(500);
        wifiConnected = (WiFi.status() == WL_CONNECTED);
        if (wifiConnected) {
            strncpy(wifiIP, WiFi.localIP().toString().c_str(), sizeof(wifiIP));
            setupOTA();
        }
    }

    if (wifiConnected) ArduinoOTA.handle();

    if (wifiConnected && (millis() - lastFetch > REFRESH_MS || lastFetch == 0)) {
        fetchData();
    }

    if (wifiConnected && (millis() - lastOllamaFetch > OLLAMA_REFRESH_MS || lastOllamaFetch == 0)) {
        fetchOllamaAndGpu();
    }

    bool btn1 = digitalRead(BTN_1) == HIGH;
    bool btn2 = digitalRead(BTN_2) == LOW;

    bool pageChanged = false, toggleBacklight = false;
    uint8_t newPageIndex = currentPageIndex;
    buttonActions::poll(btn1, btn2, currentPageIndex, layoutEngine::enabledPageCount(),
                        pageChanged, newPageIndex, toggleBacklight);
    if (pageChanged) currentPageIndex = newPageIndex;
    if (toggleBacklight) backlightOn = !backlightOn;

    drawScreen();

    if (!backlightOn) {
        ledcWrite(0, 0);
    } else {
        bool modelRunning = ollamaDataValid
            && strcmp(ollamaModel, "(idle)") != 0
            && strcmp(ollamaModel, "") != 0
            && !ollamaError;
        if (modelRunning) {
            uint32_t phase = millis() % 1200;
            int duty = (phase < 600) ? 48 : 255;
            ledcWrite(0, duty);
        } else {
            ledcWrite(0, 255);
        }
    }

    delay(100);
}
