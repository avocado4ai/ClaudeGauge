#include "button_actions.h"
#include "layout_engine.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

namespace buttonActions {

static const char* BUTTONS_PATH = "/buttons.json";
static const unsigned long LONG_PRESS_MS = 600;
static const unsigned long ACTION_DEBOUNCE_MS = 300;

struct ButtonMap {
    String btn1Short = "next_page";
    String btn2Short = "next_page";
    String btn1Long = "none";
    String btn2Long = "toggle_backlight";
};

static ButtonMap g_map;

struct PressState {
    bool lastLevel = false;
    unsigned long pressStart = 0;
    bool longFired = false;
};
static PressState g_btn1, g_btn2;
static unsigned long g_lastActionMs = 0;

static bool validAction(const String& a) {
    if (a == "none" || a == "next_page" || a == "prev_page" || a == "toggle_backlight") return true;
    if (a.startsWith("goto_page:") && a.length() > 10) return true;
    return false;
}

bool applyJson(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    ButtonMap parsed;
    parsed.btn1Short = String((const char*)(doc["btn1_short"] | "next_page"));
    parsed.btn2Short = String((const char*)(doc["btn2_short"] | "next_page"));
    parsed.btn1Long  = String((const char*)(doc["btn1_long"]  | "none"));
    parsed.btn2Long  = String((const char*)(doc["btn2_long"]  | "toggle_backlight"));

    if (!validAction(parsed.btn1Short) || !validAction(parsed.btn2Short) ||
        !validAction(parsed.btn1Long)  || !validAction(parsed.btn2Long)) {
        return false;
    }
    g_map = parsed;
    return true;
}

String toJson() {
    JsonDocument doc;
    doc["btn1_short"] = g_map.btn1Short;
    doc["btn2_short"] = g_map.btn2Short;
    doc["btn1_long"]  = g_map.btn1Long;
    doc["btn2_long"]  = g_map.btn2Long;
    String out;
    serializeJson(doc, out);
    return out;
}

bool save() {
    fs::File f = LittleFS.open(BUTTONS_PATH, "w");
    if (!f) return false;
    String json = toJson();
    size_t written = f.print(json);
    f.close();
    return written == json.length();
}

void begin() {
    g_map = ButtonMap();
    if (LittleFS.exists(BUTTONS_PATH)) {
        fs::File f = LittleFS.open(BUTTONS_PATH, "r");
        if (f) {
            String json = f.readString();
            f.close();
            if (!applyJson(json)) {
                Serial.println("buttonActions: /buttons.json invalid, using built-in default");
                g_map = ButtonMap();
            }
        }
    }
}

static void fireAction(const String& action, uint8_t currentPageIndex, uint8_t pageCount,
                        bool& outPageChanged, uint8_t& outNewPageIndex, bool& outToggleBacklight) {
    if (action == "next_page" && pageCount > 0) {
        outPageChanged = true;
        outNewPageIndex = (currentPageIndex + 1) % pageCount;
    } else if (action == "prev_page" && pageCount > 0) {
        outPageChanged = true;
        outNewPageIndex = (currentPageIndex + pageCount - 1) % pageCount;
    } else if (action == "toggle_backlight") {
        outToggleBacklight = true;
    } else if (action.startsWith("goto_page:")) {
        String id = action.substring(10);
        int idx = layoutEngine::enabledPageIndexById(id);
        if (idx >= 0) {
            outPageChanged = true;
            outNewPageIndex = (uint8_t)idx;
        }
    }
}

void poll(bool btn1Pressed, bool btn2Pressed, uint8_t currentPageIndex, uint8_t pageCount,
          bool& outPageChanged, uint8_t& outNewPageIndex, bool& outToggleBacklight) {
    outPageChanged = false;
    outNewPageIndex = currentPageIndex;
    outToggleBacklight = false;

    unsigned long now = millis();

    PressState* states[2] = { &g_btn1, &g_btn2 };
    bool levels[2] = { btn1Pressed, btn2Pressed };
    const String* shortActions[2] = { &g_map.btn1Short, &g_map.btn2Short };
    const String* longActions[2] = { &g_map.btn1Long, &g_map.btn2Long };

    for (int i = 0; i < 2; i++) {
        PressState& st = *states[i];
        bool pressed = levels[i];

        if (pressed && !st.lastLevel) {
            st.pressStart = now;
            st.longFired = false;
        } else if (pressed && st.lastLevel && !st.longFired &&
                   (now - st.pressStart) >= LONG_PRESS_MS) {
            st.longFired = true;
            if (now - g_lastActionMs > ACTION_DEBOUNCE_MS) {
                g_lastActionMs = now;
                fireAction(*longActions[i], outPageChanged ? outNewPageIndex : currentPageIndex,
                           pageCount, outPageChanged, outNewPageIndex, outToggleBacklight);
            }
        } else if (!pressed && st.lastLevel) {
            if (!st.longFired && now - g_lastActionMs > ACTION_DEBOUNCE_MS) {
                g_lastActionMs = now;
                fireAction(*shortActions[i], outPageChanged ? outNewPageIndex : currentPageIndex,
                           pageCount, outPageChanged, outNewPageIndex, outToggleBacklight);
            }
        }
        st.lastLevel = pressed;
    }
}

} // namespace buttonActions
