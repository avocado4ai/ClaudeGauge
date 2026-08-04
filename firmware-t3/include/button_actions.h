#pragma once
#include <Arduino.h>

// ============================================================
// JSON-driven button -> action mapping (/buttons.json on LittleFS).
//
// Replaces the hardcoded "either button = next screen" behavior in
// main.cpp's loop() with a configurable short-press / long-press action
// per physical button, editable from the management web page.
// ============================================================

namespace buttonActions {

// Loads /buttons.json from LittleFS, falling back to the built-in default
// (both buttons short-press to next page) if missing or invalid.
void begin();

// Re-parses and replaces the in-memory mapping from a JSON string (as
// posted to POST /api/buttons). Returns false on invalid input, leaving
// the current in-memory mapping untouched.
bool applyJson(const String& json);

// Persists the current in-memory mapping to /buttons.json.
bool save();

// Serializes the current in-memory mapping back to a JSON string.
String toJson();

// Call every loop() iteration with the current raw button levels
// (already normalized to "pressed" booleans) and the current/total page
// index. Handles debouncing and short/long press detection internally.
// On an action firing, sets outPageChanged (+ outNewPageIndex) and/or
// outToggleBacklight; main.cpp applies the side effects.
void poll(bool btn1Pressed, bool btn2Pressed, uint8_t currentPageIndex, uint8_t pageCount,
          bool& outPageChanged, uint8_t& outNewPageIndex, bool& outToggleBacklight);

} // namespace buttonActions
