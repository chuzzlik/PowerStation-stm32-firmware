#include "MosfetOutput.h"
#include "Config.h"

void MosfetOutput::begin(uint8_t pin) {
    this->pin = pin;

    if (pin == 255) {
        enabled = true;
        return;
    }

    pinMode(pin, OUTPUT);
    disable();
}

void MosfetOutput::enable() {
    enabled = true;

    if (pin != 255) {
        digitalWrite(pin, Config::MOSFET_ACTIVE_HIGH ? HIGH : LOW);
    }
}

void MosfetOutput::disable() {
    enabled = false;

    if (pin != 255) {
        digitalWrite(pin, Config::MOSFET_ACTIVE_HIGH ? LOW : HIGH);
    }
}

bool MosfetOutput::isEnabled() const {
    return enabled;
}
