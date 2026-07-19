#include "ButtonInput.h"
#include "Config.h"

void Button::begin(uint8_t pin, ButtonEvent shortEvent, ButtonEvent longEvent) {
    this->pin = pin;
    this->shortEvent = shortEvent;
    this->longEvent = longEvent;

    pinMode(pin, INPUT_PULLUP);

    stablePressed = readPressed();
    lastRawPressed = stablePressed;
    lastRawChangeMs = millis();
}

ButtonEvent Button::update() {
    bool rawPressed = readPressed();
    uint32_t now = millis();

    if (rawPressed != lastRawPressed) {
        lastRawPressed = rawPressed;
        lastRawChangeMs = now;
    }

    if (now - lastRawChangeMs < Config::BUTTON_DEBOUNCE_MS) {
        return ButtonEvent::None;
    }

    if (stablePressed != rawPressed) {
        stablePressed = rawPressed;

        if (stablePressed) {
            pressStartMs = now;
            longSent = false;
        } else {
            if (!longSent) {
                return shortEvent;
            }
        }
    }

    if (stablePressed && !longSent) {
        if (now - pressStartMs >= Config::BUTTON_LONG_PRESS_MS) {
            longSent = true;
            return longEvent;
        }
    }

    return ButtonEvent::None;
}

bool Button::readPressed() const {
    return digitalRead(pin) == LOW;
}

void ButtonInput::begin() {
    powerButton.begin(
        Config::PIN_POWER_BUTTON,
        ButtonEvent::PowerShort,
        ButtonEvent::PowerLong
    );

    screenButton.begin(
        Config::PIN_SCREEN_BUTTON,
        ButtonEvent::ScreenShort,
        ButtonEvent::ScreenLong
    );
}

ButtonEvent ButtonInput::update() {
    ButtonEvent event = powerButton.update();

    if (event != ButtonEvent::None) {
        return event;
    }

    return screenButton.update();
}
