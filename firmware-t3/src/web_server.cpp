#include "web_server.h"
#include "layout_engine.h"
#include "button_actions.h"
#include <LittleFS.h>
#include <Update.h>

namespace mgmtServer {

static const char* AUTH_USER = "admin";

static String contentTypeFor(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".json")) return "application/json";
    return "text/plain";
}

// Serves a file straight from LittleFS if present; returns false if not
// found so the caller can fall back to a built-in response.
static bool serveStaticFile(WebServer& server, const String& path) {
    if (!LittleFS.exists(path)) return false;
    fs::File f = LittleFS.open(path, "r");
    if (!f) return false;
    server.streamFile(f, contentTypeFor(path));
    f.close();
    return true;
}

static bool requireAuth(WebServer& server, const String& otaPassword) {
    if (server.authenticate(AUTH_USER, otaPassword.c_str())) return true;
    server.requestAuthentication();
    return false;
}

// Shared between the /update completion handler and its upload (ufn)
// handler — see begin() below.
static bool s_uploadAuthorized = false;

void begin(WebServer& server, Preferences& prefs, TFT_eSPI& display, String& otaPassword) {
    // ---- Dashboard / static assets ----
    server.on("/", HTTP_GET, [&server]() {
        if (serveStaticFile(server, "/index.html")) return;
        server.send(200, "text/html",
            "<h2>ClaudeGauge</h2><p>Frontend not found on LittleFS. "
            "Run <code>pio run -e t-display -t uploadfs</code>.</p>");
    });

    server.onNotFound([&server]() {
        String path = server.uri();
        if (serveStaticFile(server, path)) return;
        server.send(404, "text/plain", "404");
    });

    // ---- WiFi / proxy / session key setup ----
    server.on("/save", HTTP_POST, [&server, &prefs, &otaPassword]() {
        if (!requireAuth(server, otaPassword)) return;
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        String sk = server.arg("sessionkey");
        String pu = server.arg("proxyurl");
        String ota = server.arg("otapass");
        if (!ssid.isEmpty()) prefs.putString("wifi_ssid", ssid);
        if (!pass.isEmpty()) prefs.putString("wifi_pass", pass);
        if (!sk.isEmpty())   prefs.putString("session_key", sk);
        if (!pu.isEmpty())   prefs.putString("proxy_url", pu);
        if (!ota.isEmpty())  prefs.putString("ota_pass", ota);
        server.send(200, "text/html", "<h2>Saved! Rebooting...</h2>");
        delay(1000);
        ESP.restart();
    });

    // ---- OTA firmware upload (web path) ----
    s_uploadAuthorized = false;
    server.on("/update", HTTP_POST, [&server, &otaPassword]() {
        // The 401 challenge must be sent here, from the normal completion
        // handler — sending it from inside the upload (ufn) callback below
        // desyncs WebServer's chunked-upload parser mid-stream and the
        // connection gets reset instead of returning a clean 401.
        if (!s_uploadAuthorized) {
            requireAuth(server, otaPassword);
            return;
        }
        if (Update.hasError()) {
            server.send(200, "text/html", "<h2>Update failed</h2>");
        } else {
            server.send(200, "text/html", "<h2>Update OK, rebooting...</h2>");
            delay(500);
            ESP.restart();
        }
    }, [&server, &display, &otaPassword]() {
        // Basic Auth is only checked (never challenged) at the start of the
        // upload; once authorized, subsequent WRITE/END chunks for the same
        // upload are trusted. If unauthorized, the upload body is silently
        // drained (Update.begin() is skipped) and the 401 is sent from the
        // completion handler above once the client finishes streaming.
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            s_uploadAuthorized = server.authenticate(AUTH_USER, otaPassword.c_str());
            if (!s_uploadAuthorized) return;
            Serial.printf("OTA web upload: %s\n", upload.filename.c_str());
            display.fillScreen(TFT_BLACK);
            display.setTextDatum(MC_DATUM);
            display.setTextFont(2);
            display.setTextColor(TFT_ORANGE, TFT_BLACK);
            display.drawString("Web OTA update...", 120, 60);
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (!s_uploadAuthorized) return;
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (!s_uploadAuthorized) return;
            if (Update.end(true)) {
                Serial.printf("OTA web upload success: %u bytes\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
        }
    });

    // ---- Pages/Buttons editor API ----
    server.on("/api/layout", HTTP_GET, [&server, &otaPassword]() {
        if (!requireAuth(server, otaPassword)) return;
        server.send(200, "application/json", layoutEngine::toJson());
    });
    server.on("/api/layout", HTTP_POST, [&server, &otaPassword]() {
        if (!requireAuth(server, otaPassword)) return;
        String body = server.arg("plain");
        if (!layoutEngine::applyJson(body) || !layoutEngine::save()) {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid layout\"}");
            return;
        }
        server.send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/buttons", HTTP_GET, [&server, &otaPassword]() {
        if (!requireAuth(server, otaPassword)) return;
        server.send(200, "application/json", buttonActions::toJson());
    });
    server.on("/api/buttons", HTTP_POST, [&server, &otaPassword]() {
        if (!requireAuth(server, otaPassword)) return;
        String body = server.arg("plain");
        if (!buttonActions::applyJson(body) || !buttonActions::save()) {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid buttons\"}");
            return;
        }
        server.send(200, "application/json", "{\"ok\":true}");
    });
}

} // namespace mgmtServer
