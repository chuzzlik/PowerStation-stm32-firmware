#pragma once

#include <Arduino.h>

class MosfetOutput {
public:
    void begin(uint8_t pin);
    void enable();
    void disable();
    bool isEnabled() const;

private:
    uint8_t pin = 255;
    bool enabled = false;
};
