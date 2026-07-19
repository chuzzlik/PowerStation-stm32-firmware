#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

class SmallOled128x32 : public Adafruit_GFX {
public:
    SmallOled128x32();

    bool begin(TwoWire *wire, uint8_t address);
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;

    void clearDisplay();
    void display();

    void sleep();
    void wake();
    bool isSleeping() const;

private:
    static constexpr uint16_t WIDTH = 128;
    static constexpr uint16_t HEIGHT = 32;
    static constexpr uint16_t BUFFER_SIZE = WIDTH * HEIGHT / 8;

    TwoWire *wire = nullptr;
    uint8_t address = 0x3C;
    bool sleeping = true;

    uint8_t buffer[BUFFER_SIZE];

    bool writeCommand(uint8_t cmd);
    bool writeCommandList(const uint8_t *commands, uint8_t count);
    bool writeDataBlock(const uint8_t *data, uint8_t count);
};
