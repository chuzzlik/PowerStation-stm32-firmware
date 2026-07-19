#include "BatteryMeter.h"
#include "Config.h"
#include "Utils.h"
#include <math.h>

void BatteryMeter::begin(const BatteryConfig &config, float initialStoredWh) {
    this->config = config;

    state.learnedCapacityWh = this->config.learnedCapacityWh;
    state.currentStoredWh = clampFloat(initialStoredWh, 0.0f, state.learnedCapacityWh);
    state.learningActive = false;
    state.learningDischargeWh = 0.0f;
    state.learnCycle = LearnCycle::Idle;
    state.learnStatus = LearnStatus::Inactive;

    updateSoc();
    resetEtaAverage();
    updateEstimatedTime(millis());

    hasLastSample = false;
}

void BatteryMeter::update(const PowerSample &sample) {
    state.voltageV = sample.voltageV;
    state.currentA = sample.currentA;
    state.powerW = sample.powerW;

    updatePowerState();

    if (hasLastSample) {
        uint32_t dtMs = sample.timeMs - lastSample.timeMs;

        if (dtMs > 0 && dtMs < 60000) {
            float dtHours = static_cast<float>(dtMs) / 3600000.0f;
            integrateEnergy(sample, dtHours);
        }
    }

    if (isFullChargeDetected()) {
        markFullCharge();
    }

    if (state.learningActive && isLearningFinished()) {
        finishLearning(sample.timeMs);
    }

    updateSoc();
    updateAveragedPower(sample.timeMs);
    updateEstimatedTime(sample.timeMs);

    lastSample = sample;
    hasLastSample = true;
}

BatteryConfig BatteryMeter::getConfig() const {
    return config;
}

BatteryState BatteryMeter::getState() const {
    return state;
}

BatteryServiceInfo BatteryMeter::getServiceInfo() const {
    return service;
}

void BatteryMeter::setConfig(const BatteryConfig &config) {
    this->config = config;
    this->config.chargeEfficiency = clampFloat(this->config.chargeEfficiency, 0.50f, 1.0f);
    this->config.learningCorrectionAlpha = clampFloat(this->config.learningCorrectionAlpha, 0.01f, 1.0f);
    this->config.etaAveragingSeconds = clampFloat(this->config.etaAveragingSeconds, 5.0f, 300.0f);
    this->config.etaIdleHoldSeconds = clampFloat(this->config.etaIdleHoldSeconds, 0.0f, 120.0f);
    this->config.etaMinPowerW = clampFloat(this->config.etaMinPowerW, Config::POWER_DEADZONE_W, 100.0f);
    this->config.etaMaxHours = clampFloat(this->config.etaMaxHours, 1.0f, 1000.0f);
    state.learnedCapacityWh = this->config.learnedCapacityWh;
    state.currentStoredWh = clampFloat(state.currentStoredWh, 0.0f, state.learnedCapacityWh);

    updateSoc();
    updateEstimatedTime(millis());
}

void BatteryMeter::setServiceInfo(const BatteryServiceInfo &service) {
    this->service = service;
}

void BatteryMeter::setLearnedCapacityWh(float capacityWh) {
    config.learnedCapacityWh = clampFloat(
        capacityWh,
        config.minAllowedCapacityWh,
        config.maxAllowedCapacityWh
    );

    state.learnedCapacityWh = config.learnedCapacityWh;
    state.currentStoredWh = clampFloat(state.currentStoredWh, 0.0f, state.learnedCapacityWh);

    updateSoc();
    updateEstimatedTime(millis());
}

void BatteryMeter::setCurrentStoredWh(float wh) {
    state.currentStoredWh = clampFloat(wh, 0.0f, state.learnedCapacityWh);

    updateSoc();
    updateEstimatedTime(millis());
}

void BatteryMeter::setRemainingPercent(float percent) {
    percent = clampFloat(percent, 0.0f, 100.0f);
    state.currentStoredWh = state.learnedCapacityWh * percent / 100.0f;

    updateSoc();
    updateEstimatedTime(millis());
}

void BatteryMeter::markFullCharge() {
    state.currentStoredWh = state.learnedCapacityWh;
    state.outputDisabledByProtection = false;

    if (!state.learningActive) {
        startLearning(hasLastSample ? lastSample.timeMs : millis());
    }

    updateSoc();
    updateEstimatedTime(millis());
}

void BatteryMeter::resetLearning() {
    state.learningActive = false;
    state.learningDischargeWh = 0.0f;
    state.learnCycle = LearnCycle::Idle;
    state.learnStatus = LearnStatus::Inactive;

    service.lastMeasuredCapacityWh = 0.0f;
    service.lastCorrectionWh = 0.0f;
    service.lastCorrectionPercent = 0.0f;
}

void BatteryMeter::markOutputDisabledByProtection() {
    state.outputDisabledByProtection = true;
}

void BatteryMeter::clearOutputDisabledByProtection() {
    state.outputDisabledByProtection = false;
}

void BatteryMeter::integrateEnergy(const PowerSample &sample, float dtHours) {
    float powerW = sample.powerW;

    if (state.powerState == PowerState::Discharge) {
        float usedWh = fabsf(powerW) * dtHours;

        state.currentStoredWh -= usedWh;
        state.learningDischargeWh += usedWh;

        service.totalDischargeWh += usedWh;
    } else if (state.powerState == PowerState::Charge) {
        float chargedWh = powerW * dtHours * config.chargeEfficiency;

        state.currentStoredWh += chargedWh;
        service.totalChargeWh += chargedWh;
    }

    state.currentStoredWh = clampFloat(state.currentStoredWh, 0.0f, state.learnedCapacityWh);
}

void BatteryMeter::updatePowerState() {
    if (state.powerW > Config::POWER_DEADZONE_W) {
        state.powerState = PowerState::Charge;
        state.learnCycle = LearnCycle::Charge;
        return;
    }

    if (state.powerW < -Config::POWER_DEADZONE_W) {
        state.powerState = PowerState::Discharge;
        state.learnCycle = LearnCycle::Discharge;
        return;
    }

    state.powerState = PowerState::Idle;
    state.learnCycle = LearnCycle::Idle;
}

void BatteryMeter::updateSoc() {
    if (state.learnedCapacityWh <= 0.0f) {
        state.socPercent = 0.0f;
        return;
    }

    state.socPercent = clampFloat(
        state.currentStoredWh * 100.0f / state.learnedCapacityWh,
        0.0f,
        100.0f
    );
}

void BatteryMeter::resetEtaAverage() {
    etaAverageReady = false;
    etaAverageState = PowerState::Idle;
    averagedPowerMagnitudeW = 0.0f;
    lastEtaAverageUpdateMs = 0;
    etaIdleSinceMs = 0;
    state.averagedPowerW = 0.0f;

    displayedEtaReady = false;
    displayedEtaHours = -1.0f;
    lastEtaDisplayUpdateMs = 0;
    state.estimatedTimeHours = -1.0f;
}

void BatteryMeter::updateAveragedPower(uint32_t nowMs) {
    if (state.powerState == PowerState::Idle) {
        if (etaIdleSinceMs == 0) {
            etaIdleSinceMs = nowMs;
        }

        uint32_t holdMs = static_cast<uint32_t>(config.etaIdleHoldSeconds * 1000.0f);
        if (etaAverageReady && nowMs - etaIdleSinceMs < holdMs) {
            state.averagedPowerW = etaAverageState == PowerState::Discharge
                ? -averagedPowerMagnitudeW
                : averagedPowerMagnitudeW;
            return;
        }

        resetEtaAverage();
        return;
    }

    etaIdleSinceMs = 0;
    float magnitudeW = fabsf(state.powerW);

    if (magnitudeW < config.etaMinPowerW) {
        return;
    }

    if (!etaAverageReady || etaAverageState != state.powerState) {
        etaAverageReady = true;
        etaAverageState = state.powerState;
        averagedPowerMagnitudeW = magnitudeW;
        lastEtaAverageUpdateMs = nowMs;
        displayedEtaReady = false;
    } else {
        float dtSeconds = lastEtaAverageUpdateMs == 0
            ? 0.25f
            : static_cast<float>(nowMs - lastEtaAverageUpdateMs) / 1000.0f;
        dtSeconds = clampFloat(dtSeconds, 0.001f, 5.0f);

        // EMA с постоянной времени: при 45 сек краткие скачки нагрузки почти не меняют ETA.
        float alpha = 1.0f - expf(-dtSeconds / config.etaAveragingSeconds);
        averagedPowerMagnitudeW += alpha * (magnitudeW - averagedPowerMagnitudeW);
        lastEtaAverageUpdateMs = nowMs;
    }

    state.averagedPowerW = state.powerState == PowerState::Discharge
        ? -averagedPowerMagnitudeW
        : averagedPowerMagnitudeW;
}

void BatteryMeter::updateEstimatedTime(uint32_t nowMs) {
    if (state.powerState == PowerState::Idle) {
        // При краткой паузе сохраняем последнее значение, чтобы экран не мигал и не пересчитывался с нуля.
        if (etaAverageReady && etaIdleSinceMs != 0 &&
            nowMs - etaIdleSinceMs < static_cast<uint32_t>(config.etaIdleHoldSeconds * 1000.0f)) {
            return;
        }

        state.estimatedTimeHours = -1.0f;
        return;
    }

    if (!etaAverageReady || averagedPowerMagnitudeW < config.etaMinPowerW) {
        state.estimatedTimeHours = -1.0f;
        displayedEtaReady = false;
        return;
    }

    float rawHours = -1.0f;

    if (state.powerState == PowerState::Discharge) {
        rawHours = state.currentStoredWh / averagedPowerMagnitudeW;
    } else if (state.powerState == PowerState::Charge) {
        float effectiveChargePowerW = averagedPowerMagnitudeW * config.chargeEfficiency;
        float missingWh = state.learnedCapacityWh - state.currentStoredWh;

        if (missingWh <= 0.0f) {
            rawHours = 0.0f;
        } else if (effectiveChargePowerW >= config.etaMinPowerW) {
            rawHours = missingWh / effectiveChargePowerW;
        }
    }

    if (rawHours < 0.0f) {
        state.estimatedTimeHours = -1.0f;
        displayedEtaReady = false;
        return;
    }

    rawHours = clampFloat(rawHours, 0.0f, config.etaMaxHours);

    if (!displayedEtaReady) {
        displayedEtaReady = true;
        displayedEtaHours = quantizeEtaHours(rawHours);
        lastEtaDisplayUpdateMs = nowMs;
    } else if (nowMs - lastEtaDisplayUpdateMs >= Config::ETA_DISPLAY_REFRESH_MS) {
        displayedEtaHours = quantizeEtaHours(rawHours);
        lastEtaDisplayUpdateMs = nowMs;
    }

    state.estimatedTimeHours = clampFloat(displayedEtaHours, 0.0f, config.etaMaxHours);
}

float BatteryMeter::quantizeEtaHours(float hours) const {
    float minutes = hours * 60.0f;
    float stepMinutes = 1.0f;

    if (minutes > 600.0f) {
        stepMinutes = 10.0f;
    } else if (minutes > 120.0f) {
        stepMinutes = 5.0f;
    }

    float roundedMinutes = roundf(minutes / stepMinutes) * stepMinutes;
    return roundedMinutes / 60.0f;
}

bool BatteryMeter::isFullChargeDetected() const {
    bool voltageIsFull = state.voltageV >= config.fullVoltageV;
    bool currentIsLow = fabsf(state.currentA) <= config.fullCurrentA;
    bool isChargingOrIdle = state.powerState == PowerState::Charge || state.powerState == PowerState::Idle;

    return voltageIsFull && currentIsLow && isChargingOrIdle;
}

bool BatteryMeter::isLearningFinished() const {
    bool voltageIsLow = state.voltageV > 1.0f && state.voltageV <= config.learningEndVoltageV;
    bool enoughEnergyMeasured = state.learningDischargeWh >= config.learningMinDischargeWh;
    bool isDischarging = state.powerState == PowerState::Discharge;

    return voltageIsLow && enoughEnergyMeasured && isDischarging;
}

void BatteryMeter::startLearning(uint32_t timeMs) {
    state.learningActive = true;
    state.learningDischargeWh = 0.0f;
    state.learnStatus = LearnStatus::Active;

    service.lastLearningStartedMs = timeMs;
}

void BatteryMeter::finishLearning(uint32_t timeMs) {
    state.learningActive = false;

    float measuredCapacityWh = state.learningDischargeWh;

    applyCapacityCorrection(measuredCapacityWh);

    service.learnedCycles++;
    service.lastMeasuredCapacityWh = measuredCapacityWh;
    service.lastLearningFinishedMs = timeMs;

    state.learningDischargeWh = 0.0f;
    state.learnStatus = LearnStatus::Complete;
}

void BatteryMeter::applyCapacityCorrection(float measuredCapacityWh) {
    if (measuredCapacityWh < config.minAllowedCapacityWh) {
        return;
    }

    if (measuredCapacityWh > config.maxAllowedCapacityWh) {
        return;
    }

    float oldCapacityWh = config.learnedCapacityWh;

    float newCapacityWh =
        oldCapacityWh * (1.0f - config.learningCorrectionAlpha) +
        measuredCapacityWh * config.learningCorrectionAlpha;

    config.learnedCapacityWh = newCapacityWh;
    state.learnedCapacityWh = newCapacityWh;

    service.lastCorrectionWh = newCapacityWh - oldCapacityWh;

    if (oldCapacityWh > 0.0f) {
        service.lastCorrectionPercent = service.lastCorrectionWh * 100.0f / oldCapacityWh;
    } else {
        service.lastCorrectionPercent = 0.0f;
    }

    state.currentStoredWh = clampFloat(state.currentStoredWh, 0.0f, state.learnedCapacityWh);
}
