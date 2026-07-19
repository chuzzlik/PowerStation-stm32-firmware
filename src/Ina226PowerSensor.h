#pragma once

#include <Arduino.h>
#include "Types.h"

class Ina226PowerSensor {
public:
    bool begin(uint8_t sdaPin, uint8_t sclPin, uint8_t address, float shuntOhms, bool invertCurrent);
    bool isConnected();
    PowerSample read();

private:
    uint8_t sdaPin = 255;
    uint8_t sclPin = 255;
    uint8_t address = 0x44;
    float shuntOhms = 0.0015f;
    bool invertCurrent = false;
    bool pinsReady = false;

    void i2cDelay() const;
    void sdaHigh();
    void sdaLow();
    void sclHigh();
    void sclLow();
    bool readSda();

    void i2cStart();
    void i2cStop();
    bool i2cWrite(uint8_t value);
    uint8_t i2cRead(bool sendAck);

    bool writeRegister(uint8_t reg, uint16_t value);
    uint16_t readRegister(uint8_t reg);
};
