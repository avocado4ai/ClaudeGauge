#include "touch_handler.h"
#include "pin_config.h"

#if HAS_TOUCH
#include <Wire.h>

void TouchHandler::init() {
#if defined(BOARD_C6_AMOLED)
    // FT3168 — I2C already initialized by display manager
    // No hardware reset pin on GPIO (goes through I/O expander)
    pinMode(TOUCH_INT, INPUT);

    Wire.beginTransmission(TOUCH_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("FT3168 touch controller found");
    } else {
        Serial.println("FT3168 touch controller NOT found");
    }
#else
    // CST816S — Reset the chip via dedicated GPIO
    resetChip();

    // Initialize I2C on touch-specific pins
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    // Configure interrupt pin
    pinMode(TOUCH_INT, INPUT);

    // Verify chip is present
    Wire.beginTransmission(TOUCH_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("CST816S touch controller found");
    } else {
        Serial.println("CST816S touch controller NOT found");
    }
#endif
}

void TouchHandler::resetChip() {
#if !defined(BOARD_C6_AMOLED)
    // CST816S has a dedicated reset GPIO
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);
#endif
    // FT3168: no GPIO reset available (handled by I/O expander at display init)
}

uint8_t TouchHandler::readRegister(uint8_t reg) {
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(TOUCH_ADDR, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}

bool TouchHandler::readTouch() {
    // Register 0x02 = number of touch points (same for both CST816S and FT3168)
    uint8_t points = readRegister(0x02);
    return (points > 0);
}

void TouchHandler::update() {
    _doubleTapped = false;
    _touched = false;

    bool touching = readTouch();
    uint32_t now = millis();

    // Detect finger-down edge (transition from not touching to touching)
    if (touching && !_wasTouching) {
        _touched = true;

        if (_tapCount > 0 && (now - _lastTapTime) <= DOUBLE_TAP_MS) {
            // Second tap within window -> double tap
            _doubleTapped = true;
            _tapCount = 0;
        } else {
            // First tap or tap after timeout
            _tapCount = 1;
            _lastTapTime = now;
        }
    }

    // Reset tap count if too much time passed since last tap
    if (_tapCount > 0 && (now - _lastTapTime) > DOUBLE_TAP_MS) {
        _tapCount = 0;
    }

    _wasTouching = touching;
}

bool TouchHandler::isDoubleTapped() {
    return _doubleTapped;
}

bool TouchHandler::isTouched() {
    return _touched;
}
#endif
