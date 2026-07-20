#include "Ina226PowerSensor.h"
#include <math.h>

bool Ina226PowerSensor::begin(uint8_t sdaPin, uint8_t sclPin, uint8_t address, float shuntOhms, bool invertCurrent) {
    this->sdaPin = sdaPin;
    this->sclPin = sclPin;
    this->address = address;
    this->shuntOhms = shuntOhms;
    this->invertCurrent = invertCurrent;

    pinMode(this->sdaPin, OUTPUT_OPEN_DRAIN);
    pinMode(this->sclPin, OUTPUT_OPEN_DRAIN);

    sdaHigh();
    sclHigh();
    delay(5);

    pinsReady = true;

    if (!isConnected()) {
        return false;
    }

    // AVG 16, VBUS conversion 1.1ms, VSHUNT conversion 1.1ms,
    // continuous shunt + bus.
    if (!writeRegister(0x00, 0x4527)) {
        return false;
    }

    delay(20);

    return isConnected();
}

bool Ina226PowerSensor::isConnected() {
    if (!pinsReady) {
        return false;
    }

    i2cStart();
    bool ack = i2cWrite(static_cast<uint8_t>(address << 1));
    i2cStop();

    return ack;
}

PowerSample Ina226PowerSensor::read() {
    PowerSample sample;
    sample.timeMs = millis();

    int16_t rawShunt = static_cast<int16_t>(readRegister(0x01));
    uint16_t rawBus = readRegister(0x02);

    float shuntVoltageV = rawShunt * 0.0000025f; // 2.5 uV per bit.
    float busVoltageV = rawBus * 0.00125f / 1.05f; // Компенсация измеренного завышения на 5%.

    float currentA = 0.0f;

    if (shuntOhms > 0.0f) {
        currentA = shuntVoltageV / shuntOhms;
    }

    if (invertCurrent) {
        currentA = -currentA;
    }

    sample.voltageV = busVoltageV;
    sample.currentA = currentA;
    sample.powerW = sample.voltageV * sample.currentA;

    return sample;
}

void Ina226PowerSensor::i2cDelay() const {
    delayMicroseconds(5);
}

void Ina226PowerSensor::sdaHigh() {
    digitalWrite(sdaPin, HIGH);
}

void Ina226PowerSensor::sdaLow() {
    digitalWrite(sdaPin, LOW);
}

void Ina226PowerSensor::sclHigh() {
    digitalWrite(sclPin, HIGH);
}

void Ina226PowerSensor::sclLow() {
    digitalWrite(sclPin, LOW);
}

bool Ina226PowerSensor::readSda() {
    pinMode(sdaPin, INPUT_PULLUP);
    i2cDelay();
    bool value = digitalRead(sdaPin) == HIGH;
    pinMode(sdaPin, OUTPUT_OPEN_DRAIN);
    return value;
}

void Ina226PowerSensor::i2cStart() {
    sdaHigh();
    sclHigh();
    i2cDelay();
    sdaLow();
    i2cDelay();
    sclLow();
}

void Ina226PowerSensor::i2cStop() {
    sdaLow();
    i2cDelay();
    sclHigh();
    i2cDelay();
    sdaHigh();
    i2cDelay();
}

bool Ina226PowerSensor::i2cWrite(uint8_t value) {
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (value & 0x80) {
            sdaHigh();
        } else {
            sdaLow();
        }

        i2cDelay();
        sclHigh();
        i2cDelay();
        sclLow();

        value <<= 1;
    }

    // ACK: ведомый тянет SDA вниз.
    sdaHigh();
    i2cDelay();
    sclHigh();
    bool ack = !readSda();
    sclLow();

    return ack;
}

uint8_t Ina226PowerSensor::i2cRead(bool sendAck) {
    uint8_t value = 0;

    sdaHigh();
    pinMode(sdaPin, INPUT_PULLUP);

    for (uint8_t bit = 0; bit < 8; bit++) {
        value <<= 1;

        i2cDelay();
        sclHigh();
        i2cDelay();

        if (digitalRead(sdaPin) == HIGH) {
            value |= 1;
        }

        sclLow();
    }

    pinMode(sdaPin, OUTPUT_OPEN_DRAIN);

    if (sendAck) {
        sdaLow();
    } else {
        sdaHigh();
    }

    i2cDelay();
    sclHigh();
    i2cDelay();
    sclLow();
    sdaHigh();

    return value;
}

bool Ina226PowerSensor::writeRegister(uint8_t reg, uint16_t value) {
    if (!pinsReady) {
        return false;
    }

    i2cStart();

    bool ok = true;
    ok = ok && i2cWrite(static_cast<uint8_t>(address << 1));
    ok = ok && i2cWrite(reg);
    ok = ok && i2cWrite(static_cast<uint8_t>(value >> 8));
    ok = ok && i2cWrite(static_cast<uint8_t>(value & 0xFF));

    i2cStop();

    return ok;
}

uint16_t Ina226PowerSensor::readRegister(uint8_t reg) {
    if (!pinsReady) {
        return 0;
    }

    i2cStart();

    bool ok = true;
    ok = ok && i2cWrite(static_cast<uint8_t>(address << 1));
    ok = ok && i2cWrite(reg);

    if (!ok) {
        i2cStop();
        return 0;
    }

    // Repeated START.
    i2cStart();
    ok = i2cWrite(static_cast<uint8_t>((address << 1) | 0x01));

    if (!ok) {
        i2cStop();
        return 0;
    }

    uint8_t high = i2cRead(true);   // ACK после первого байта.
    uint8_t low = i2cRead(false);   // NACK после последнего байта.

    i2cStop();

    return (static_cast<uint16_t>(high) << 8) | low;
}
