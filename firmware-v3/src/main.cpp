// ============================================================
// ClaudeGauge V3 — lcars-esp32 engine on ESP32-C6 AMOLED
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_XCA9554.h>
#include <lcars.h>

#ifdef XPOWERS_CHIP_AXP2101
  #include <XPowersLib.h>
#endif

#include "config.h"
#include "pin_config.h"
#include "data_models.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "api_client.h"
#include "claude_ai_client.h"
#include "settings_manager.h"
#include "web_server.h"

// Screen definitions
#include "screens/setup_screen.h"
#include "screens/connecting_screen.h"
#include "screens/overview_screen.h"
#include "screens/models_screen.h"
#include "screens/monthly_models_screen.h"
#include "screens/code_screen.h"
#include "screens/monthly_code_screen.h"
#include "screens/status_screen.h"
#include "screens/claudeai_screen.h"

// ============================================================
// Display hardware
// ============================================================
Arduino_ESP32QSPI* bus = nullptr;
Arduino_SH8601*    display = nullptr;

// ============================================================
// Engine + screens
// ============================================================
LcarsEngine engine;
LcarsBootScreen bootScreen;

SetupScreen        setupScreen;
ConnectingScreen   connectingScreen;
OverviewScreen     overviewScreen;
ModelsScreen       modelsScreen;
MonthlyModelsScreen monthlyModelsScreen;
CodeScreen         codeScreen;
MonthlyCodeScreen  monthlyCodeScreen;
StatusScreen       statusScreen;
ClaudeAiScreen     claudeAiScreen;

// Dashboard screen rotation — must match designer tab order
// V3 designer tabs: Claude.ai (0), Status (1), Setup (2)
LcarsScreen* dashScreens[] = {
    &claudeAiScreen, &statusScreen
};
const int DASH_SCREEN_COUNT = sizeof(dashScreens) / sizeof(dashScreens[0]);
int currentDashScreen = 0;

// ============================================================
// App state + services
// ============================================================
enum DeviceMode { MODE_SETUP, MODE_CONNECTING, MODE_DASHBOARD };

static AppState        state;
static WiFiManager     wifiMgr;
static TimeManager     timeMgr;
static ApiClient       apiClient;
static ClaudeAiClient  claudeAiClient;
static SettingsManager settingsMgr;
static ConfigWebServer webServer;
static DeviceMode      deviceMode = MODE_SETUP;

// Forward declarations
void enterSetupMode();
void enterDashboardMode();
void fetchAllData();
void updateWiFiState();
uint32_t getCountdownSec();

// ============================================================
// Display init
// ============================================================
void initDisplay() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

#ifdef XPOWERS_CHIP_AXP2101
    {
        XPowersPMU pmu;
        if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, TOUCH_SDA, TOUCH_SCL)) {
            Serial.println("[PMU] AXP2101 found");
            pmu.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
            pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
            pmu.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
            pmu.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
            pmu.enableBattDetection();
            pmu.enableBattVoltageMeasure();
        }
    }
#endif

    Adafruit_XCA9554 expander;
    if (expander.begin(IO_EXP_ADDR, &Wire)) {
        expander.pinMode(IO_EXP_PIN_LCD_EN1, OUTPUT);
        expander.pinMode(IO_EXP_PIN_LCD_EN2, OUTPUT);
        expander.digitalWrite(IO_EXP_PIN_LCD_EN1, HIGH);
        expander.digitalWrite(IO_EXP_PIN_LCD_EN2, HIGH);
    }

    bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
    display = new Arduino_SH8601(bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT);
    display->begin();
    display->fillScreen(0x0000);
    display->setBrightness(255);
}

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ClaudeGauge V3 ===");

    memset(&state, 0, sizeof(AppState));
    state.current_screen = SCREEN_OVERVIEW;
    state.uptime_start = millis();
    state.last_activity = millis();

    // Init button
    pinMode(BTN_BOOT, INPUT_PULLUP);

    // Init display + engine
    initDisplay();
    engine.begin(display, SCR_WIDTH, SCR_HEIGHT);
    engine.setTheme(LCARS_THEME_TNG);

    // Show a static splash (no animated boot — avoids flicker on direct-to-display)
    display->fillScreen(0x0000);
    LcarsFont::drawTextUpper(engine.sprite(), "CLAUDEGAUGE V3", 184, 200,
        LCARS_FONT_LG, LCARS_SUNFLOWER, LCARS_BLACK, MC_DATUM);
    LcarsFont::drawTextUpper(engine.sprite(), "INITIALIZING...", 184, 240,
        LCARS_FONT_SM, LCARS_AMBER, LCARS_BLACK, MC_DATUM);
    delay(1500);

    // Init settings
    settingsMgr.begin();

    if (!settingsMgr.hasWiFi()) {
        Serial.println("No WiFi credentials. Setup mode.");
        enterSetupMode();
    } else {
        Serial.println("Connecting...");
        deviceMode = MODE_CONNECTING;
        connectingScreen.setStatus("Connecting WiFi...");
        engine.setScreen(&connectingScreen);

        wifiMgr.connect(settingsMgr.getWiFiSSID(), settingsMgr.getWiFiPassword());
        updateWiFiState();

        if (!wifiMgr.isConnected()) {
            Serial.println("WiFi failed. Setup mode.");
            enterSetupMode();
            return;
        }

        webServer.begin(&settingsMgr);
        webServer.setEngine(&engine);

        connectingScreen.setStatus("Syncing time...");
        engine.update();
        timeMgr.syncNTP();

        // Init API clients
        apiClient.init(settingsMgr.getApiKey());

        if (settingsMgr.hasSessionKey()) {
            String proxyUrl = settingsMgr.getProxyUrl();
            if (proxyUrl.length() == 0) proxyUrl = CLAUDEAI_DEFAULT_PROXY_URL;
            claudeAiClient.init(settingsMgr.getSessionKey(), proxyUrl);
        }

        // Fetch data
        if (settingsMgr.hasApiKey() && timeMgr.isTimeSynced()) {
            connectingScreen.setStatus("Fetching data...");
            engine.update();
            fetchAllData();
        }

        enterDashboardMode();
    }
}

// ============================================================
// Mode management
// ============================================================
void enterSetupMode() {
    deviceMode = MODE_SETUP;
    webServer.startAPMode();
    webServer.begin(&settingsMgr);
    webServer.setEngine(&engine);

    setupScreen.setAPName(webServer.getAPName().c_str());
    setupScreen.setIP(WiFi.softAPIP().toString().c_str());
    engine.setScreen(&setupScreen);
}

void enterDashboardMode() {
    deviceMode = MODE_DASHBOARD;
    currentDashScreen = 0;

    // Pass state pointer to dashboard screens
    claudeAiScreen.setState(&state);
    statusScreen.setState(&state, &wifiMgr);

    engine.setScreen(dashScreens[0]);  // Claude.ai first (matches designer)
    Serial.println("Dashboard mode active.");
}

// ============================================================
// Navigation (button + touch)
// ============================================================
void handleNavigation() {
    // BOOT button: cycle screens
    if (digitalRead(BTN_BOOT) == LOW) {
        delay(200);  // debounce
        if (digitalRead(BTN_BOOT) == LOW) {
            currentDashScreen = (currentDashScreen + 1) % DASH_SCREEN_COUNT;
            engine.setScreen(dashScreens[currentDashScreen]);
            state.last_activity = millis();
        }
    }
}

// ============================================================
// Data fetching (reused from V1)
// ============================================================
void fetchAllData() {
    if (!settingsMgr.hasApiKey() || !wifiMgr.isConnected() || !timeMgr.isTimeSynced()) return;

    state.is_fetching = true;
    Serial.println("--- Fetching data ---");

    String todayStart = timeMgr.todayStartUTC();
    String todayEnd   = timeMgr.todayEndUTC();
    String monthStart = timeMgr.monthStartUTC();
    String todayDate  = timeMgr.todayDateOnly();

    static UsageData newUsage;
    static CostData newCost;
    static ClaudeCodeData newCode;
    static UsageData newMonthlyUsage;
    static ClaudeCodeData newMonthlyCode;

    // 1. Usage
    memset(&newUsage, 0, sizeof(UsageData));
    if (apiClient.fetchUsageReport(newUsage, todayStart, todayEnd)) {
        state.usage = newUsage;
    }

    // 2. Cost
    memset(&newCost, 0, sizeof(CostData));
    if (apiClient.fetchCostReport(newCost, monthStart, todayEnd, todayDate.c_str())) {
        newCost.valid = true;
        newCost.fetched_at = millis();
        // Sort by cost descending
        for (int i = 0; i < newCost.model_count - 1; i++)
            for (int j = i + 1; j < newCost.model_count; j++)
                if (newCost.models[j].cost_usd > newCost.models[i].cost_usd) {
                    ModelCost tmp = newCost.models[i];
                    newCost.models[i] = newCost.models[j];
                    newCost.models[j] = tmp;
                }
        state.cost = newCost;
    }

    // 3. Claude Code
    memset(&newCode, 0, sizeof(ClaudeCodeData));
    if (apiClient.fetchClaudeCodeReport(newCode, todayDate)) {
        state.code = newCode;
    }

    // 4. Monthly usage
    memset(&newMonthlyUsage, 0, sizeof(UsageData));
    if (apiClient.fetchUsageReport(newMonthlyUsage, monthStart, todayEnd)) {
        state.monthly_usage = newMonthlyUsage;
    }

    // 5. Monthly code
    memset(&newMonthlyCode, 0, sizeof(ClaudeCodeData));
    if (apiClient.fetchClaudeCodeReport(newMonthlyCode, timeMgr.monthStartDateOnly())) {
        state.monthly_code = newMonthlyCode;
    }

    // Fallbacks
    if (state.cost.today_usd < 0.01f && state.code.total_cost > 0)
        state.cost.today_usd = state.code.total_cost;

    if (state.cost.model_count == 0 && state.code.model_cost_count > 0) {
        for (int i = 0; i < state.code.model_cost_count && i < MAX_MODELS; i++)
            state.cost.models[i] = state.code.model_costs[i];
        state.cost.model_count = state.code.model_cost_count;
    }

    // Claude.ai
    if (settingsMgr.hasSessionKey()) {
        static ClaudeAiUsage newClaudeAi;
        strncpy(newClaudeAi.org_uuid, state.claude_ai.org_uuid, sizeof(newClaudeAi.org_uuid) - 1);
        if (claudeAiClient.fetchUsage(newClaudeAi)) {
            state.claude_ai = newClaudeAi;
        }
    }

    state.is_fetching = false;
    state.last_refresh = millis();
    state.next_refresh = state.last_refresh + REFRESH_INTERVAL_MS;
    Serial.println("--- Fetch complete ---");
    claudeAiScreen.invalidateGauges();  // Force gauge redraw with new data
}

// ============================================================
// Auto-refresh
// ============================================================
void handleAutoRefresh() {
    uint32_t now = millis();
    if (state.next_refresh > 0 && now >= state.next_refresh) {
        fetchAllData();
    }
    if (state.last_refresh == 0 && wifiMgr.isConnected() &&
        timeMgr.isTimeSynced() && !state.is_fetching) {
        fetchAllData();
    }
}

void updateWiFiState() {
    state.wifi_connected = wifiMgr.isConnected();
    state.wifi_rssi = WiFi.RSSI();
}

uint32_t getCountdownSec() {
    if (state.next_refresh == 0) return 0;
    uint32_t now = millis();
    if (now >= state.next_refresh) return 0;
    return (state.next_refresh - now) / 1000;
}

// ============================================================
// Loop
// ============================================================
void loop() {
    webServer.handleClient();

    if (webServer.shouldReboot()) {
        delay(1000);
        ESP.restart();
    }

    if (deviceMode == MODE_SETUP) {
        engine.update();
        delay(100);  // Slow updates in setup mode
        return;
    }

    // Dashboard mode
    engine.update();
    handleNavigation();
    handleAutoRefresh();
    updateWiFiState();

    if (!wifiMgr.isConnected()) {
        wifiMgr.reconnectIfNeeded();
        updateWiFiState();
    }

    delay(16);  // ~60fps cap
}
