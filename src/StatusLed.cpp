#include "StatusLed.h"
#include "Config.h"
#include <math.h>

namespace {
    // Минимальная яркость в режимах, где светодиод не должен полностью гаснуть.
    constexpr uint8_t LED_ACTIVE_MIN_BRIGHTNESS = 10;
    constexpr uint8_t DISCHARGE_BRIGHTNESS = 179; // 70% от 255.

    constexpr float PI_F = 3.1415926f;

    uint8_t scaleActiveBrightness(uint8_t value, bool allowFullOff) {
        uint8_t maxBrightness = Config::LED_MAX_BRIGHTNESS;

        if (maxBrightness == 0 || (allowFullOff && value == 0)) {
            return 0;
        }

        if (maxBrightness <= LED_ACTIVE_MIN_BRIGHTNESS) {
            return maxBrightness;
        }

        return LED_ACTIVE_MIN_BRIGHTNESS
            + ((uint16_t)value * (maxBrightness - LED_ACTIVE_MIN_BRIGHTNESS)) / 255;
    }
}

void StatusLed::begin(uint8_t pin) {
    this->pin = pin;

    if (pin == 255) {
        return;
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);

    ledcSetup(
        Config::LED_PWM_CHANNEL,
        Config::LED_PWM_FREQUENCY_HZ,
        Config::LED_PWM_RESOLUTION_BITS
    );
    ledcAttachPin(pin, Config::LED_PWM_CHANNEL);
    ledcWrite(Config::LED_PWM_CHANNEL, 0);
}

void StatusLed::setMode(StatusLedMode mode) {
    if (this->mode == mode) {
        return;
    }

    this->mode = mode;
    modeStartedMs = millis();
}

void StatusLed::update() {
    if (pin == 255) {
        return;
    }

    uint32_t now = millis();
    uint32_t elapsed = now - modeStartedMs;

    uint8_t value = 0;
    bool activeMode = true;
    bool allowFullOff = false;

    switch (mode) {
        case StatusLedMode::Off:
            value = 0;
            activeMode = false;
            break;

        case StatusLedMode::IdleBreathing:
            value = calculateBreathing(elapsed, 7000);
            break;

        case StatusLedMode::ChargeBlink:
            value = calculateSoftBlink(elapsed, 1000);
            allowFullOff = true;
            break;

        case StatusLedMode::DischargeSolid:
            value = DISCHARGE_BRIGHTNESS;
            break;

        case StatusLedMode::LowBatteryBlink:
            value = calculateBlink(elapsed, 1500);
            break;
    }

    if (activeMode) {
        value = scaleActiveBrightness(value, allowFullOff);
    }

    ledcWrite(Config::LED_PWM_CHANNEL, value);
}

uint8_t StatusLed::calculateBreathing(uint32_t elapsedMs, uint32_t periodMs) const {
    uint32_t pos = elapsedMs % periodMs;
    float phase = static_cast<float>(pos) / static_cast<float>(periodMs);

    float value = (1.0f - cosf(phase * 2.0f * PI_F)) * 0.5f;
    return static_cast<uint8_t>(value * 255.0f + 0.5f);
}

uint8_t StatusLed::calculateSoftBlink(uint32_t elapsedMs, uint32_t periodMs) const {
    uint32_t pos = elapsedMs % periodMs;

    constexpr uint32_t HOLD_MS = 420;
    constexpr uint32_t FADE_MS = 80;

    if (pos < HOLD_MS) {
        return 255;
    }

    if (pos < HOLD_MS + FADE_MS) {
        uint32_t fadePos = pos - HOLD_MS;
        return static_cast<uint8_t>(255UL * (FADE_MS - fadePos) / FADE_MS);
    }

    if (pos < HOLD_MS * 2 + FADE_MS) {
        return 0;
    }

    uint32_t fadePos = pos - (HOLD_MS * 2 + FADE_MS);
    return static_cast<uint8_t>(255UL * fadePos / FADE_MS);
}

uint8_t StatusLed::calculateBlink(uint32_t elapsedMs, uint32_t periodMs) const {
    uint32_t pos = elapsedMs % periodMs;

    if (pos < 80) {
        return 255;
    }

    if (pos >= 200 && pos < 280) {
        return 255;
    }

    return 0;
}
