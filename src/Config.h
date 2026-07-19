#pragma once

#include <Arduino.h>

namespace Config {
    constexpr const char *FIRMWARE_VERSION = "2.5.1-ble-api";

    constexpr uint8_t PIN_NONE = 255;

    // Большой OLED 128x64 SH1106, аппаратный I2C Wire.
    constexpr uint8_t DISPLAY_MAIN_SDA = 5;
    constexpr uint8_t DISPLAY_MAIN_SCL = 4;
    constexpr uint8_t DISPLAY_MAIN_ADDRESS = 0x3C;
    constexpr uint8_t DISPLAY_MAIN_WIDTH = 128;
    constexpr uint8_t DISPLAY_MAIN_HEIGHT = 64;

    // Узкий OLED 128x32 SSD1306, аппаратный I2C Wire1.
    constexpr uint8_t DISPLAY_SMALL_SDA = 16;
    constexpr uint8_t DISPLAY_SMALL_SCL = 15;
    constexpr uint8_t DISPLAY_SMALL_ADDRESS = 0x3C;
    constexpr uint8_t DISPLAY_SMALL_WIDTH = 128;
    constexpr uint8_t DISPLAY_SMALL_HEIGHT = 32;

    // INA226 на программном I2C, чтобы оба аппаратных I2C были заняты дисплеями.
    constexpr uint8_t SHUNT_SDA = 7;
    constexpr uint8_t SHUNT_SCL = 6;
    constexpr uint8_t INA226_ADDRESS = 0x44;

    // Шунт 50A 75mV: R = 0.075V / 50A = 0.0015 Ohm.
    constexpr float SHUNT_OHMS = 0.0015f;

    // Если заряд/разряд перепутаны местами — поменять значение.
    constexpr bool INVERT_CURRENT = true;

    constexpr uint8_t PIN_POWER_LED = 10;
    constexpr uint8_t PIN_POWER_BUTTON = 11;
    constexpr uint8_t PIN_SCREEN_BUTTON = 12;
    constexpr uint8_t PIN_MOSFET_OUTPUT = 21;

    // Большинство таких MOSFET-модулей бывают active-low: LOW = включено, HIGH = выключено.
    // Если после прошивки логика окажется обратной, поменять на true.
    constexpr bool MOSFET_ACTIVE_HIGH = true;

    constexpr float POWER_DEADZONE_W = 2.0f;

    constexpr uint32_t SMALL_SCREEN_OFF_PREVIEW_MS = 2000;
    constexpr float AUTO_POWER_ON_MIN_CHARGE_W = 1.0f;
    constexpr uint32_t AUTO_POWER_ON_CHARGE_DELAY_MS = 1000;
    constexpr uint32_t ETA_DISPLAY_REFRESH_MS = 10000;

    constexpr uint32_t SENSOR_REFRESH_MS = 250;
    constexpr uint32_t DISPLAY_REFRESH_MS = 200;
    constexpr uint32_t ANIMATION_REFRESH_MS = 200;
    constexpr uint32_t BLE_NOTIFY_MS = 1000;
    constexpr uint16_t BLE_MTU = 517;
    constexpr size_t BLE_MAX_VALUE_BYTES = 500;

    constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
    constexpr uint32_t BUTTON_LONG_PRESS_MS = 900;

    constexpr uint8_t LED_MAX_BRIGHTNESS = 255;

    constexpr const char *BLE_NAME = "PowerBank";
}
