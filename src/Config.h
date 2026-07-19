#pragma once

#include <Arduino.h>

namespace Config {
    constexpr const char *FIRMWARE_VERSION = "2.6.2";

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

    constexpr uint8_t PIN_FAN_PWM = 13;

    // Медный NTC на радиаторе подключён к GPIO2,
    // пластиковый NTC на выходе воздуха — к GPIO1.
    constexpr uint8_t PIN_NTC_POWER = 2;
    constexpr uint8_t PIN_NTC_AIR = 1;

    // Большинство таких MOSFET-модулей бывают active-low: LOW = включено, HIGH = выключено.
    // Если после прошивки логика окажется обратной, поменять на true.
    constexpr bool MOSFET_ACTIVE_HIGH = true;

    constexpr float POWER_DEADZONE_W = 2.0f;

    constexpr uint32_t SMALL_SCREEN_OFF_PREVIEW_MS = 2000;
    constexpr float AUTO_POWER_ON_MIN_CHARGE_W = 1.0f;
    constexpr uint32_t AUTO_POWER_ON_CHARGE_DELAY_MS = 1000;
    constexpr uint32_t SYSTEM_IDLE_TIMEOUT_DEFAULT_SEC = 15UL * 60UL;
    constexpr uint32_t SYSTEM_IDLE_TIMEOUT_MAX_SEC = 24UL * 60UL * 60UL;
    constexpr uint32_t ETA_DISPLAY_REFRESH_MS = 10000;

    constexpr uint32_t SENSOR_REFRESH_MS = 250;
    constexpr uint32_t DISPLAY_REFRESH_MS = 200;
    constexpr uint32_t ANIMATION_REFRESH_MS = 200;
    constexpr uint32_t BLE_NOTIFY_MS = 1000;
    constexpr uint32_t BLE_WAITING_TIMEOUT_MS = 5UL * 60UL * 1000UL;
    constexpr uint16_t BLE_MTU = 517;
    constexpr size_t BLE_MAX_VALUE_BYTES = 512;

    constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
    constexpr uint32_t BUTTON_LONG_PRESS_MS = 900;

    // Светодиод и вентилятор используют разные LEDC-каналы.
    constexpr uint8_t LED_PWM_CHANNEL = 6;
    constexpr uint32_t LED_PWM_FREQUENCY_HZ = 5000;
    constexpr uint8_t LED_PWM_RESOLUTION_BITS = 8;
    constexpr uint8_t LED_MAX_BRIGHTNESS = 255;

    // Сглаживание только показания мощности на маленьком OLED.
    constexpr float SMALL_POWER_SMOOTH_TAU_SECONDS = 1.6f;
    constexpr float SMALL_POWER_FAST_TAU_SECONDS = 0.35f;
    constexpr float SMALL_POWER_FAST_DELTA_W = 25.0f;

    // Автоматическая калибровка КПД зарядки.
    constexpr float CHARGE_EFFICIENCY_MIN = 0.50f;
    constexpr float CHARGE_EFFICIENCY_MAX = 1.00f;
    constexpr float CHARGE_EFFICIENCY_ALPHA = 0.25f;
    constexpr float CHARGE_CALIBRATION_MIN_CAPACITY_PERCENT = 10.0f;
    constexpr float CHARGE_CALIBRATION_MIN_INPUT_WH = 20.0f;
    constexpr float CHARGE_SOC_RESERVE_PERCENT = 0.5f;
    constexpr uint32_t FULL_CHARGE_HOLD_MS = 30000;

    // Вентилятор 12 В через IRL3705.
    constexpr uint8_t FAN_PWM_CHANNEL = 7;
    constexpr uint32_t FAN_PWM_FREQUENCY_HZ = 250;
    constexpr uint8_t FAN_PWM_RESOLUTION_BITS = 8;
    constexpr float FAN_MIN_PERCENT = 50.0f;
    constexpr float FAN_START_PERCENT = 100.0f;
    constexpr uint32_t FAN_START_BOOST_MS = 1000;
    constexpr float FAN_OFF_TEMPERATURE_C = 38.0f;
    constexpr float FAN_ON_TEMPERATURE_C = 42.0f;
    constexpr float FAN_FULL_TEMPERATURE_C = 60.0f;
    constexpr float FAN_RAMP_PERCENT_PER_SECOND = 25.0f;

    // NTC 10k B3950. Делитель: постоянный резистор к 3.3 В, NTC к GND.
    constexpr float NTC_NOMINAL_RESISTANCE_OHM = 10000.0f;
    constexpr float NTC_FIXED_RESISTANCE_OHM = 10000.0f;
    constexpr float NTC_BETA = 3950.0f;
    constexpr float NTC_NOMINAL_TEMPERATURE_C = 25.0f;
    constexpr uint16_t NTC_ADC_MAX = 4095;
    constexpr uint16_t NTC_ADC_FAULT_MARGIN = 8;
    constexpr float NTC_MIN_PLAUSIBLE_C = -20.0f;
    constexpr float NTC_MAX_PLAUSIBLE_C = 150.0f;
    constexpr uint32_t NTC_REFRESH_MS = 250;
    constexpr float NTC_SMOOTH_TAU_SECONDS = 1.5f;

    // Скорректировано по повторному измерению при температуре помещения 21.4 °C:
    // Tpower = 19.4 °C, Tair = 23.5 °C.
    constexpr float NTC_POWER_OFFSET_C = -3.2f;
    constexpr float NTC_AIR_OFFSET_C = -5.5f;

    constexpr const char *BLE_NAME = "PowerBank";
}
