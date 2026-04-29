// ============================================================
// Battery Test for Waveshare ESP32-C6-Touch-AMOLED-1.8
//
// SAFETY: This sketch ONLY configures battery charging parameters.
// It does NOT touch DC1, DC3, BLDO1, ALDOs or any power rails.
// The AXP2101 factory OTP defaults handle all power rails correctly.
//
// What it does:
//   - Init I2C, AXP2101, I/O expander, AMOLED display
//   - Display battery voltage, charge %, USB status, temperature
//   - Update once per second
//
// Flash via USB. Then unplug USB, connect battery, press PWR
// button — board should boot from battery.
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_XCA9554.h>
#include <XPowersLib.h>
#include <Arduino_GFX_Library.h>

// Color aliases (Arduino_GFX uses RGB565_* prefix; some sketches use plain names)
#ifndef BLACK
  #define BLACK    0x0000
  #define WHITE    0xFFFF
  #define RED      0xF800
  #define GREEN    0x07E0
  #define BLUE     0x001F
  #define YELLOW   0xFFE0
  #define CYAN     0x07FF
  #define MAGENTA  0xF81F
  #define ORANGE   0xFD20
  #define DARKGREY 0x7BEF
#endif

// ---------- Pin definitions (from Waveshare schematic) ----------
#define LCD_SCLK    0
#define LCD_SDIO0   1
#define LCD_SDIO1   2
#define LCD_SDIO2   3
#define LCD_SDIO3   4
#define LCD_CS      5

#define LCD_WIDTH   368
#define LCD_HEIGHT  448

#define I2C_SDA     8
#define I2C_SCL     7

#define IO_EXP_ADDR 0x20
#define IO_EXP_LCD_EN1  4
#define IO_EXP_LCD_EN2  5

// ---------- Globals ----------
XPowersPMU      pmu;
bool            pmuOk = false;
Adafruit_XCA9554 expander;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

Arduino_SH8601 *gfx = new Arduino_SH8601(
    bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("==========================================");
    Serial.println("  ESP32-C6 AMOLED 1.8 - Battery Test");
    Serial.println("==========================================");

    // ---- I2C ----
    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println("[I2C] Initialized");

    // ---- AXP2101 PMIC (CHARGING CONFIG ONLY — no rail changes!) ----
    if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        pmuOk = true;
        Serial.println("[PMU] AXP2101 found");
        Serial.printf("[PMU] Chip ID: 0x%02X\n", pmu.getChipID());

        // Configure charging only
        pmu.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
        pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

        // Enable battery monitoring & detection
        pmu.enableBattDetection();
        pmu.enableBattVoltageMeasure();
        pmu.enableVbusVoltageMeasure();
        pmu.enableSystemVoltageMeasure();
        pmu.enableTemperatureMeasure();

        // Set precharge current and termination current (helps with detection)
        pmu.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
        pmu.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

        Serial.println("[PMU] Charging configured (200mA, 4.2V target)");
    } else {
        Serial.println("[PMU] AXP2101 NOT FOUND — running on USB direct");
    }

    // ---- I/O Expander (enable display) ----
    if (expander.begin(IO_EXP_ADDR, &Wire)) {
        Serial.println("[EXP] TCA9554 found, enabling display power");
        expander.pinMode(IO_EXP_LCD_EN1, OUTPUT);
        expander.pinMode(IO_EXP_LCD_EN2, OUTPUT);
        expander.digitalWrite(IO_EXP_LCD_EN1, HIGH);
        expander.digitalWrite(IO_EXP_LCD_EN2, HIGH);
        delay(100);
    } else {
        Serial.println("[EXP] TCA9554 NOT FOUND — display will not work");
    }

    // ---- Display ----
    if (gfx->begin()) {
        Serial.println("[LCD] SH8601 initialized");
        gfx->fillScreen(BLACK);
        gfx->setBrightness(255);
        gfx->setTextSize(2);
        gfx->setCursor(20, 20);
        gfx->setTextColor(YELLOW);
        gfx->println("BATTERY TEST");
    } else {
        Serial.println("[LCD] SH8601 init FAILED");
    }

    Serial.println("[BOOT] Setup complete");
    Serial.println();
}

void drawBatteryStatus() {
    if (!pmuOk) {
        gfx->fillRect(0, 60, LCD_WIDTH, 200, BLACK);
        gfx->setCursor(20, 80);
        gfx->setTextColor(RED);
        gfx->setTextSize(2);
        gfx->println("PMU NOT FOUND");
        return;
    }

    uint16_t  vBat   = pmu.getBattVoltage();      // mV
    int       pct    = pmu.getBatteryPercent();
    uint16_t  vBus   = pmu.getVbusVoltage();      // mV
    uint16_t  vSys   = pmu.getSystemVoltage();    // mV
    float     temp   = pmu.getTemperature();      // C
    bool      usb    = pmu.isVbusIn();
    bool      batt   = pmu.isBatteryConnect();
    bool      chg    = pmu.isCharging();

    // Clear status area
    gfx->fillRect(0, 60, LCD_WIDTH, 380, BLACK);

    int y = 80;
    gfx->setTextSize(2);

    auto row = [&](const char* label, const String& val, uint16_t color) {
        gfx->setCursor(20, y);
        gfx->setTextColor(WHITE);
        gfx->print(label);
        gfx->setTextColor(color);
        gfx->println(val);
        y += 28;
    };

    row("Battery: ", batt ? "YES" : "NO",     batt ? GREEN : RED);
    row("USB:     ", usb  ? "YES" : "NO",     usb  ? GREEN : DARKGREY);
    row("Chrg:    ", chg  ? "YES" : "NO",     chg  ? GREEN : DARKGREY);
    y += 10;
    row("VBat:    ", String(vBat) + " mV",    YELLOW);
    row("Pct:     ", String(pct)  + " %",     YELLOW);
    row("VBus:    ", String(vBus) + " mV",    CYAN);
    row("VSys:    ", String(vSys) + " mV",    CYAN);
    row("Temp:    ", String(temp, 1) + " C",  ORANGE);

    // Serial output too
    Serial.printf("[STAT] Bat:%s USB:%s Chg:%s VBat:%dmV %d%% VBus:%dmV T:%.1fC\n",
                  batt ? "Y" : "N", usb ? "Y" : "N", chg ? "Y" : "N",
                  vBat, pct, vBus, temp);
}

void loop() {
    drawBatteryStatus();
    delay(1000);
}
