#pragma once

#include <Arduino.h>
#include "Types.h"

class StatusLed {
public:
    void begin(uint8_t pin);
    void setMode(StatusLedMode mode);
    void update();

private:
    uint8_t pin = 255;
    StatusLedMode mode = StatusLedMode::Off;
    uint32_t modeStartedMs = 0;

    uint8_t calculateBreathing(uint32_t elapsedMs, uint32_t periodMs) const;
    uint8_t calculateSoftBlink(uint32_t elapsedMs, uint32_t periodMs) const;
    uint8_t calculateBlink(uint32_t elapsedMs, uint32_t periodMs) const;
};
