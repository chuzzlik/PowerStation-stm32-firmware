#include "CoolingController.h"
#include "Config.h"
#include "Utils.h"
#include <math.h>

void CoolingController::begin() {
    pinMode(Config::PIN_FAN_PWM, OUTPUT);
    digitalWrite(Config::PIN_FAN_PWM, LOW);

    analogReadResolution(12);
    analogSetPinAttenuation(Config::PIN_NTC_POWER, ADC_11db);
    analogSetPinAttenuation(Config::PIN_NTC_AIR, ADC_11db);

    ledcSetup(
        Config::FAN_PWM_CHANNEL,
        Config::FAN_PWM_FREQUENCY_HZ,
        Config::FAN_PWM_RESOLUTION_BITS
    );
    ledcAttachPin(Config::PIN_FAN_PWM, Config::FAN_PWM_CHANNEL);
    ledcWrite(Config::FAN_PWM_CHANNEL, 0);

    lastSensorReadMs = millis() - Config::NTC_REFRESH_MS;
    lastFanUpdateMs = millis();
    update();
}

void CoolingController::update() {
    uint32_t now = millis();

    if (now - lastSensorReadMs >= Config::NTC_REFRESH_MS) {
        float dtSeconds = lastSensorReadMs == 0
            ? static_cast<float>(Config::NTC_REFRESH_MS) / 1000.0f
            : static_cast<float>(now - lastSensorReadMs) / 1000.0f;
        dtSeconds = clampFloat(dtSeconds, 0.001f, 5.0f);
        lastSensorReadMs = now;

        float powerTemperatureC = NAN;
        float airTemperatureC = NAN;

        state.powerSensorValid = readTemperature(
            Config::PIN_NTC_POWER,
            Config::NTC_POWER_OFFSET_C,
            powerTemperatureC
        );
        state.airSensorValid = readTemperature(
            Config::PIN_NTC_AIR,
            Config::NTC_AIR_OFFSET_C,
            airTemperatureC
        );

        if (state.powerSensorValid) {
            state.powerTemperatureC = smoothTemperature(
                state.powerTemperatureC,
                powerTemperatureC,
                dtSeconds,
                powerTemperatureReady
            );
        } else {
            powerTemperatureReady = false;
            state.powerTemperatureC = NAN;
        }

        if (state.airSensorValid) {
            state.airTemperatureC = smoothTemperature(
                state.airTemperatureC,
                airTemperatureC,
                dtSeconds,
                airTemperatureReady
            );
        } else {
            airTemperatureReady = false;
            state.airTemperatureC = NAN;
        }

        state.fault = !state.powerSensorValid || !state.airSensorValid;
    }

    updateFan(now);
}

CoolingState CoolingController::getState() const {
    return state;
}

bool CoolingController::readTemperature(
    uint8_t pin,
    float offsetC,
    float &temperatureC
) const {
    uint32_t rawSum = 0;

    for (uint8_t i = 0; i < 4; i++) {
        rawSum += analogRead(pin);
    }

    float raw = static_cast<float>(rawSum) / 4.0f;

    if (
        raw <= Config::NTC_ADC_FAULT_MARGIN ||
        raw >= Config::NTC_ADC_MAX - Config::NTC_ADC_FAULT_MARGIN
    ) {
        return false;
    }

    // Делитель: постоянный резистор к 3.3 В, NTC к GND.
    float ntcResistance = Config::NTC_FIXED_RESISTANCE_OHM
        * raw
        / (static_cast<float>(Config::NTC_ADC_MAX) - raw);

    if (!isfinite(ntcResistance) || ntcResistance <= 0.0f) {
        return false;
    }

    float nominalKelvin = Config::NTC_NOMINAL_TEMPERATURE_C + 273.15f;
    float inverseKelvin = 1.0f / nominalKelvin
        + logf(ntcResistance / Config::NTC_NOMINAL_RESISTANCE_OHM) / Config::NTC_BETA;

    if (!isfinite(inverseKelvin) || inverseKelvin <= 0.0f) {
        return false;
    }

    temperatureC = 1.0f / inverseKelvin - 273.15f + offsetC;

    return isfinite(temperatureC)
        && temperatureC >= Config::NTC_MIN_PLAUSIBLE_C
        && temperatureC <= Config::NTC_MAX_PLAUSIBLE_C;
}

float CoolingController::smoothTemperature(
    float previousC,
    float currentC,
    float dtSeconds,
    bool &ready
) const {
    if (!ready || !isfinite(previousC)) {
        ready = true;
        return currentC;
    }

    float alpha = 1.0f - expf(-dtSeconds / Config::NTC_SMOOTH_TAU_SECONDS);
    return previousC + alpha * (currentC - previousC);
}

float CoolingController::calculateTargetFanPercent(float temperatureC) const {
    if (temperatureC >= Config::FAN_FULL_TEMPERATURE_C) {
        return 100.0f;
    }

    if (temperatureC <= Config::FAN_ON_TEMPERATURE_C) {
        return Config::FAN_MIN_PERCENT;
    }

    float rangeC = Config::FAN_FULL_TEMPERATURE_C - Config::FAN_ON_TEMPERATURE_C;
    float position = (temperatureC - Config::FAN_ON_TEMPERATURE_C) / rangeC;

    return Config::FAN_MIN_PERCENT
        + position * (100.0f - Config::FAN_MIN_PERCENT);
}

void CoolingController::updateFan(uint32_t nowMs) {
    float dtSeconds = lastFanUpdateMs == 0
        ? static_cast<float>(Config::NTC_REFRESH_MS) / 1000.0f
        : static_cast<float>(nowMs - lastFanUpdateMs) / 1000.0f;
    dtSeconds = clampFloat(dtSeconds, 0.001f, 5.0f);
    lastFanUpdateMs = nowMs;

    if (state.fault) {
        fanRequested = true;
        startupBoostActive = false;
        targetFanPercent = 100.0f;
        actualFanPercent = 100.0f;
        writeFanPercent(actualFanPercent);
        return;
    }

    float controlTemperatureC = state.powerTemperatureC;

    if (controlTemperatureC >= Config::FAN_ON_TEMPERATURE_C) {
        fanRequested = true;
    } else if (controlTemperatureC < Config::FAN_OFF_TEMPERATURE_C) {
        fanRequested = false;
    }

    if (!fanRequested) {
        startupBoostActive = false;
        targetFanPercent = 0.0f;
        actualFanPercent = 0.0f;
        writeFanPercent(0.0f);
        return;
    }

    targetFanPercent = calculateTargetFanPercent(controlTemperatureC);

    if (actualFanPercent <= 0.0f && !startupBoostActive) {
        startupBoostActive = true;
        startupBoostUntilMs = nowMs + Config::FAN_START_BOOST_MS;
        actualFanPercent = Config::FAN_START_PERCENT;
        writeFanPercent(actualFanPercent);
        return;
    }

    if (startupBoostActive) {
        if (static_cast<int32_t>(nowMs - startupBoostUntilMs) < 0) {
            actualFanPercent = Config::FAN_START_PERCENT;
            writeFanPercent(actualFanPercent);
            return;
        }

        startupBoostActive = false;
    }

    float maxStep = Config::FAN_RAMP_PERCENT_PER_SECOND * dtSeconds;

    if (actualFanPercent < targetFanPercent) {
        actualFanPercent = min(actualFanPercent + maxStep, targetFanPercent);
    } else if (actualFanPercent > targetFanPercent) {
        actualFanPercent = max(actualFanPercent - maxStep, targetFanPercent);
    }

    actualFanPercent = clampFloat(actualFanPercent, Config::FAN_MIN_PERCENT, 100.0f);
    writeFanPercent(actualFanPercent);
}

void CoolingController::writeFanPercent(float percent) {
    percent = clampFloat(percent, 0.0f, 100.0f);

    uint8_t duty = percent <= 0.0f
        ? 0
        : static_cast<uint8_t>(roundf(percent * 255.0f / 100.0f));

    ledcWrite(Config::FAN_PWM_CHANNEL, duty);

    state.fanPercent = static_cast<uint8_t>(roundf(percent));
    state.fanOn = duty > 0;
}
