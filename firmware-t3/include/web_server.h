#pragma once
#include <WebServer.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

// ============================================================
// On-device management web page: WiFi/proxy/session-key setup, OTA
// firmware upload, and the Pages/Buttons JSON API consumed by the visual
// editor served from LittleFS (data/index.html + app.js + style.css).
//
// HTTP Basic Auth (username "admin", password = the device's OTA password,
// default "claudegauge") protects every state-changing / firmware-flashing
// route: POST /save, POST /update, POST /api/layout, POST /api/buttons,
// GET /api/layout, GET /api/buttons.
// ============================================================

namespace mgmtServer {

// Registers all routes on `server` and enables Basic Auth using the current
// value of `otaPassword`. `display` is used for on-screen OTA progress
// feedback during a blocking web firmware upload.
void begin(WebServer& server, Preferences& prefs, TFT_eSPI& display, String& otaPassword);

} // namespace mgmtServer
