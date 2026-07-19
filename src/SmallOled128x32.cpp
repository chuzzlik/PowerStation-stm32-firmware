#include "SmallOled128x32.h"
#include <string.h>

SmallOled128x32::SmallOled128x32()
    : Adafruit_GFX(WIDTH, HEIGHT) {
    memset(buffer, 0, sizeof(buffer));
}

bool SmallOled128x32::begin(TwoWire *wire, uint8_t address) {
    this->wire = wire;
    this->address = address;

    if (this->wire == nullptr) {
        return false;
    }

    this->wire->beginTransmission(this->address);
    if (this->wire->endTransmission() != 0) {
        return false;
    }

    delay(10);

    // Базовая инициализация SSD1306 128x32.
    const uint8_t initCommands[] = {
        0xAE,        // display off
        0xD5, 0x80,  // display clock
        0xA8, 0x1F,  // multiplex 1/32
        0xD3, 0x00,  // display offset
        0x40,        // start line
        0x8D, 0x14,  // charge pump on
        0x20, 0x00,  // horizontal addressing
        0xA1,        // segment remap
        0xC8,        // COM scan dec
        0xDA, 0x02,  // COM pins for 128x32
        0x81, 0x8F,  // contrast
        0xD9, 0xF1,  // pre-charge
        0xDB, 0x40,  // vcomh
        0xA4,        // resume RAM content
        0xA6,        // normal display
        0x2E,        // deactivate scroll
        0xAF         // display on
    };

    if (!writeCommandList(initCommands, sizeof(initCommands))) {
        return false;
    }

    sleeping = false;

    clearDisplay();
    display();

    return true;
}

void SmallOled128x32::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return;
    }

    uint16_t index = x + (y / 8) * WIDTH;
    uint8_t mask = 1 << (y & 7);

    if (color) {
        buffer[index] |= mask;
    } else {
        buffer[index] &= ~mask;
    }
}

void SmallOled128x32::clearDisplay() {
    memset(buffer, 0, sizeof(buffer));
}

void SmallOled128x32::display() {
    if (sleeping || wire == nullptr) {
        return;
    }

    writeCommand(0x21); // column address
    writeCommand(0);
    writeCommand(WIDTH - 1);

    writeCommand(0x22); // page address
    writeCommand(0);
    writeCommand(3);

    for (uint16_t i = 0; i < BUFFER_SIZE; i += 16) {
        writeDataBlock(&buffer[i], 16);
    }
}

void SmallOled128x32::sleep() {
    if (wire == nullptr) {
        sleeping = true;
        return;
    }

    clearDisplay();
    display();
    writeCommand(0xAE);
    sleeping = true;
}

void SmallOled128x32::wake() {
    if (wire == nullptr) {
        return;
    }

    writeCommand(0xAF);
    sleeping = false;
}

bool SmallOled128x32::isSleeping() const {
    return sleeping;
}

bool SmallOled128x32::writeCommand(uint8_t cmd) {
    if (wire == nullptr) {
        return false;
    }

    wire->beginTransmission(address);
    wire->write(0x00);
    wire->write(cmd);
    return wire->endTransmission() == 0;
}

bool SmallOled128x32::writeCommandList(const uint8_t *commands, uint8_t count) {
    if (wire == nullptr) {
        return false;
    }

    wire->beginTransmission(address);
    wire->write(0x00);

    for (uint8_t i = 0; i < count; i++) {
        wire->write(commands[i]);
    }

    return wire->endTransmission() == 0;
}

bool SmallOled128x32::writeDataBlock(const uint8_t *data, uint8_t count) {
    if (wire == nullptr) {
        return false;
    }

    wire->beginTransmission(address);
    wire->write(0x40);

    for (uint8_t i = 0; i < count; i++) {
        wire->write(data[i]);
    }

    return wire->endTransmission() == 0;
}
