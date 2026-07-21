#include "BatteryMeter.h"
#include "Config.h"
#include "Utils.h"
#include <math.h>

void BatteryMeter::begin(const BatteryConfig &config, float initialStoredWh) {
    this->config = config;
    sanitizeConfig();

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
    fullConditionActive = false;
    fullChargeLatched = false;
    chargeCycleActive = false;
    chargeCycleValid = false;
    chargeCycleInputWh = 0.0f;
    configChanged = false;
}

void BatteryMeter::update(const PowerSample &sample) {
    state.voltageV = sample.voltageV;
    state.currentA = sample.currentA;
    state.powerW = sample.powerW;

    PowerState previousState = state.powerState;
    updatePowerState();
    updateChargeCycle(previousState);

    if (hasLastSample) {
        uint32_t dtMs = sample.timeMs - lastSample.timeMs;

        if (dtMs > 0 && dtMs < 60000) {
            float dtHours = static_cast<float>(dtMs) / 3600000.0f;
            integrateEnergy(sample, dtHours);
        }
    }

    updateFullChargeDetection(sample.timeMs);

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
    sanitizeConfig();

    state.learnedCapacityWh = this->config.learnedCapacityWh;
    state.currentStoredWh = clampFloat(state.currentStoredWh, 0.0f, state.learnedCapacityWh);

    updateSoc();
    updateEstimatedTime(millis());
}

void BatteryMeter::sanitizeConfig() {
    this->config.minAllowedCapacityWh = Config::LEARNED_CAPACITY_MIN_WH;
    this->config.maxAllowedCapacityWh = Config::LEARNED_CAPACITY_MAX_WH;
    this->config.learnedCapacityWh = clampFloat(
        this->config.learnedCapacityWh,
        this->config.minAllowedCapacityWh,
        this->config.maxAllowedCapacityWh
    );
    this->config.chargeEfficiency = clampFloat(
        this->config.chargeEfficiency,
        Config::CHARGE_EFFICIENCY_MIN,
        Config::CHARGE_EFFICIENCY_MAX
    );
    this->config.lowCutVoltageV = clampFloat(
        this->config.lowCutVoltageV,
        Config::LOW_CUT_VOLTAGE_MIN_V,
        Config::LOW_CUT_VOLTAGE_MAX_V
    );
    this->config.fullVoltageV = clampFloat(
        this->config.fullVoltageV,
        Config::FULL_VOLTAGE_MIN_V,
        Config::FULL_VOLTAGE_MAX_V
    );
    this->config.fullCurrentA = clampFloat(
        this->config.fullCurrentA,
        Config::FULL_CURRENT_MIN_A,
        Config::FULL_CURRENT_MAX_A
    );
    this->config.lowSocPercent = clampFloat(
        this->config.lowSocPercent,
        Config::LOW_SOC_PERCENT_MIN,
        Config::LOW_SOC_PERCENT_MAX
    );
    this->config.learningCorrectionAlpha = clampFloat(
        this->config.learningCorrectionAlpha,
        Config::LEARNING_CORRECTION_ALPHA_MIN,
        Config::LEARNING_CORRECTION_ALPHA_MAX
    );
    this->config.etaAveragingSeconds = clampFloat(
        this->config.etaAveragingSeconds,
        Config::ETA_AVERAGING_MIN_SECONDS,
        Config::ETA_AVERAGING_MAX_SECONDS
    );
    if (!Config::isEtaAveragingAllowed(this->config.etaAveragingSeconds)) {
        this->config.etaAveragingSeconds = BatteryConfig().etaAveragingSeconds;
    }
    this->config.etaIdleHoldSeconds = clampFloat(
        this->config.etaIdleHoldSeconds,
        Config::ETA_IDLE_HOLD_MIN_SECONDS,
        Config::ETA_IDLE_HOLD_MAX_SECONDS
    );
    this->config.powerLimitW = clampFloat(
        this->config.powerLimitW,
        Config::POWER_LIMIT_MIN_W,
        Config::POWER_LIMIT_MAX_W
    );
    this->config.etaMinPowerW = clampFloat(this->config.etaMinPowerW, Config::POWER_DEADZONE_W, 100.0f);
    this->config.etaMaxHours = clampFloat(this->config.etaMaxHours, 1.0f, 1000.0f);
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
    fullChargeLatched = state.currentStoredWh >= state.learnedCapacityWh;

    updateSoc();
    updateEstimatedTime(millis());
}

void BatteryMeter::setRemainingPercent(float percent) {
    percent = clampFloat(percent, 0.0f, 100.0f);
    state.currentStoredWh = state.learnedCapacityWh * percent / 100.0f;
    fullChargeLatched = percent >= 100.0f;

    updateSoc();
    updateEstimatedTime(millis());
}

void BatteryMeter::markFullCharge() {
    state.currentStoredWh = state.learnedCapacityWh;
    state.outputDisabledByProtection = false;
    fullChargeLatched = true;
    fullConditionActive = false;

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

bool BatteryMeter::consumeConfigChanged() {
    bool changed = configChanged;
    configChanged = false;
    return changed;
}

void BatteryMeter::integrateEnergy(const PowerSample &sample, float dtHours) {
    float powerW = sample.powerW;

    if (state.powerState == PowerState::Discharge) {
        float usedWh = fabsf(powerW) * dtHours;

        state.currentStoredWh -= usedWh;
        state.learningDischargeWh += usedWh;

        service.totalDischargeWh += usedWh;
    } else if (state.powerState == PowerState::Charge) {
        float inputWh = powerW * dtHours;
        float chargedWh = inputWh * config.chargeEfficiency;

        if (chargeCycleActive && chargeCycleValid) {
            chargeCycleInputWh += inputWh;
        }

        state.currentStoredWh += chargedWh;
        service.totalChargeWh += chargedWh;

        if (!fullChargeLatched) {
            float reserveWh = state.learnedCapacityWh
                * Config::CHARGE_SOC_RESERVE_PERCENT
                / 100.0f;
            float maxBeforeFullWh = max(0.0f, state.learnedCapacityWh - reserveWh);
            state.currentStoredWh = min(state.currentStoredWh, maxBeforeFullWh);
        }
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

            if (!fullChargeLatched && rawHours > 0.0f) {
                rawHours = max(rawHours, 1.0f / 60.0f);
            }
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

void BatteryMeter::updateChargeCycle(PowerState previousState) {
    if (state.powerState == PowerState::Discharge) {
        chargeCycleActive = false;
        chargeCycleValid = false;
        chargeCycleInputWh = 0.0f;
        fullConditionActive = false;
        fullChargeLatched = false;
        return;
    }

    if (state.powerState == PowerState::Charge && previousState != PowerState::Charge) {
        if (!chargeCycleActive) {
            chargeCycleActive = true;
            chargeCycleValid = true;
            chargeCycleStartStoredWh = state.currentStoredWh;
            chargeCycleInputWh = 0.0f;
            fullChargeLatched = false;
        }
    }
}

void BatteryMeter::updateFullChargeDetection(uint32_t nowMs) {
    if (fullChargeLatched) {
        return;
    }

    if (!isFullChargeCondition()) {
        fullConditionActive = false;
        fullConditionStartedMs = 0;
        return;
    }

    if (!fullConditionActive) {
        fullConditionActive = true;
        fullConditionStartedMs = nowMs;
        return;
    }

    if (nowMs - fullConditionStartedMs >= Config::FULL_CHARGE_HOLD_MS) {
        completeAutomaticFullCharge(nowMs);
    }
}

void BatteryMeter::completeAutomaticFullCharge(uint32_t nowMs) {
    applyChargeEfficiencyCalibration();

    chargeCycleActive = false;
    chargeCycleValid = false;
    chargeCycleInputWh = 0.0f;

    markFullCharge();
    service.lastLearningStartedMs = nowMs;
}

void BatteryMeter::applyChargeEfficiencyCalibration() {
    if (!chargeCycleActive || !chargeCycleValid || state.learnedCapacityWh <= 0.0f) {
        return;
    }

    float missingAtStartWh = state.learnedCapacityWh - chargeCycleStartStoredWh;
    float minMissingWh = state.learnedCapacityWh
        * Config::CHARGE_CALIBRATION_MIN_CAPACITY_PERCENT
        / 100.0f;

    if (
        missingAtStartWh < minMissingWh ||
        chargeCycleInputWh < Config::CHARGE_CALIBRATION_MIN_INPUT_WH
    ) {
        return;
    }

    float measuredEfficiency = missingAtStartWh / chargeCycleInputWh;

    if (
        !isfinite(measuredEfficiency) ||
        measuredEfficiency < Config::CHARGE_EFFICIENCY_MIN ||
        measuredEfficiency > Config::CHARGE_EFFICIENCY_MAX
    ) {
        return;
    }

    float oldEfficiency = config.chargeEfficiency;
    float newEfficiency = oldEfficiency * (1.0f - Config::CHARGE_EFFICIENCY_ALPHA)
        + measuredEfficiency * Config::CHARGE_EFFICIENCY_ALPHA;

    newEfficiency = clampFloat(
        newEfficiency,
        Config::CHARGE_EFFICIENCY_MIN,
        Config::CHARGE_EFFICIENCY_MAX
    );

    if (fabsf(newEfficiency - oldEfficiency) >= 0.001f) {
        config.chargeEfficiency = newEfficiency;
        configChanged = true;
    }
}

bool BatteryMeter::isFullChargeCondition() const {
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
