#pragma once

#include <Arduino.h>
#include "Types.h"

class Button {
public:
    void begin(uint8_t pin, ButtonEvent shortEvent, ButtonEvent longEvent);
    ButtonEvent update();

private:
    uint8_t pin = 255;

    ButtonEvent shortEvent = ButtonEvent::None;
    ButtonEvent longEvent = ButtonEvent::None;

    bool stablePressed = false;
    bool lastRawPressed = false;
    bool longSent = false;

    uint32_t lastRawChangeMs = 0;
    uint32_t pressStartMs = 0;

    bool readPressed() const;
};

class ButtonInput {
public:
    void begin();
    ButtonEvent update();

private:
    Button powerButton;
    Button screenButton;
};
