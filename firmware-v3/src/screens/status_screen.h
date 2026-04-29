#pragma once
#include <lcars.h>
#include "data_models.h"
#include "data_bindings.h"
#include "wifi_manager.h"
#include "screen_layouts_v3.h"

class StatusScreen : public LcarsScreen {
public:
    const char* title() const override { return "DIAGNOSTICS"; }
    uint32_t refreshIntervalMs() const override { return 30000; }  // Redraw every 30s
    void setState(const AppState* s, WiFiManager* w) { _state = s; _wifi = w; }

    void onDraw(LcarsCanvas& spr, const LcarsFrame::Rect& c) override {
        if (!_state) return;

        // No content clear — overdraw in place to avoid AMOLED flicker.

#ifdef V3_ST_NET_X
        LcarsWidgets::drawLabel(spr, V3_ST_NET_X, V3_ST_NET_Y, "NETWORK", _theme->accent);
#endif
#ifdef V3_ST_WIFI_X
        LcarsWidgets::drawStatusRow(spr, V3_ST_WIFI_X, V3_ST_WIFI_Y, V3_ST_WIFI_W,
            "WIFI", _state->wifi_connected ? "CONNECTED" : "OFFLINE",
            _state->wifi_connected ? _theme->statusOk : _theme->statusErr,
            _theme->textDim);
#endif
#ifdef V3_ST_RSSI_X
        if (_state->wifi_connected) {
            char rssi[16];
            snprintf(rssi, sizeof(rssi), "%d dBm", _state->wifi_rssi);
            LcarsWidgets::drawStatusRow(spr, V3_ST_RSSI_X, V3_ST_RSSI_Y, V3_ST_RSSI_W,
                "SIGNAL", rssi, _theme->text, _theme->textDim);
        }
#endif
#ifdef V3_ST_IP_X
        if (_state->wifi_connected) {
            LcarsWidgets::drawStatusRow(spr, V3_ST_IP_X, V3_ST_IP_Y, V3_ST_IP_W,
                "IP", WiFi.localIP().toString().c_str(), _theme->text, _theme->textDim);
        }
#endif

#ifdef V3_ST_SEP1_X
        LcarsWidgets::drawSeparator(spr, V3_ST_SEP1_X, V3_ST_SEP1_Y, V3_ST_SEP1_W, _theme->accent);
#endif
#ifdef V3_ST_API_X
        LcarsWidgets::drawLabel(spr, V3_ST_API_X, V3_ST_API_Y, "API", _theme->accent);
#endif
#ifdef V3_ST_APIST_X
        LcarsWidgets::drawStatusRow(spr, V3_ST_APIST_X, V3_ST_APIST_Y, V3_ST_APIST_W,
            "STATUS", _state->api_error ? "ERROR" : "OK",
            _state->api_error ? _theme->statusErr : _theme->statusOk,
            _theme->textDim);
#endif
#ifdef V3_ST_FETCH_X
        if (_state->last_refresh > 0) {
            uint32_t ago = (millis() - _state->last_refresh) / 1000;
            char agoBuf[16];
            snprintf(agoBuf, sizeof(agoBuf), "%lus ago", (unsigned long)ago);
            LcarsWidgets::drawStatusRow(spr, V3_ST_FETCH_X, V3_ST_FETCH_Y, V3_ST_FETCH_W,
                "LAST FETCH", agoBuf, _theme->text, _theme->textDim);
        }
#endif

#ifdef V3_ST_SEP2_X
        LcarsWidgets::drawSeparator(spr, V3_ST_SEP2_X, V3_ST_SEP2_Y, V3_ST_SEP2_W, _theme->accent);
#endif
#ifdef V3_ST_SYS_X
        LcarsWidgets::drawLabel(spr, V3_ST_SYS_X, V3_ST_SYS_Y, "SYSTEM", _theme->accent);
#endif
#ifdef V3_ST_UP_X
        {
            uint32_t upSec = (millis() - _state->uptime_start) / 1000;
            char uptimeBuf[24];
            if (upSec >= 3600)
                snprintf(uptimeBuf, sizeof(uptimeBuf), "%luh %lum", upSec/3600, (upSec%3600)/60);
            else
                snprintf(uptimeBuf, sizeof(uptimeBuf), "%lum %lus", upSec/60, upSec%60);
            LcarsWidgets::drawStatusRow(spr, V3_ST_UP_X, V3_ST_UP_Y, V3_ST_UP_W,
                "UPTIME", uptimeBuf, _theme->text, _theme->textDim);
        }
#endif
#ifdef V3_ST_HEAP_X
        {
            char heapBuf[16];
            snprintf(heapBuf, sizeof(heapBuf), "%luK", (unsigned long)(ESP.getFreeHeap() / 1024));
            LcarsWidgets::drawStatusRow(spr, V3_ST_HEAP_X, V3_ST_HEAP_Y, V3_ST_HEAP_W,
                "FREE HEAP", heapBuf, _theme->text, _theme->textDim);
        }
#endif

#ifdef V3_ST_CASCADE_X
        LcarsWidgets::drawDataCascade(spr, V3_ST_CASCADE_X, V3_ST_CASCADE_Y,
            V3_ST_CASCADE_W, V3_ST_CASCADE_H, _theme->textDim);
#endif
    }

private:
    const AppState* _state = nullptr;
    WiFiManager* _wifi = nullptr;
};
