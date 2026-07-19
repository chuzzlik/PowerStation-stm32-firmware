#pragma once

#include <Arduino.h>
#include "Types.h"

class BatteryMeter {
public:
    void begin(const BatteryConfig &config, float initialStoredWh);
    void update(const PowerSample &sample);

    BatteryConfig getConfig() const;
    BatteryState getState() const;
    BatteryServiceInfo getServiceInfo() const;

    void setConfig(const BatteryConfig &config);
    void setServiceInfo(const BatteryServiceInfo &service);

    void setLearnedCapacityWh(float capacityWh);
    void setCurrentStoredWh(float wh);
    void setRemainingPercent(float percent);

    void markFullCharge();
    void resetLearning();

    void markOutputDisabledByProtection();
    void clearOutputDisabledByProtection();

    bool consumeConfigChanged();

private:
    BatteryConfig config;
    BatteryState state;
    BatteryServiceInfo service;

    bool hasLastSample = false;
    PowerSample lastSample;

    bool etaAverageReady = false;
    PowerState etaAverageState = PowerState::Idle;
    float averagedPowerMagnitudeW = 0.0f;
    uint32_t lastEtaAverageUpdateMs = 0;
    uint32_t etaIdleSinceMs = 0;

    bool displayedEtaReady = false;
    float displayedEtaHours = -1.0f;
    uint32_t lastEtaDisplayUpdateMs = 0;

    bool fullConditionActive = false;
    bool fullChargeLatched = false;
    uint32_t fullConditionStartedMs = 0;

    bool chargeCycleActive = false;
    bool chargeCycleValid = false;
    float chargeCycleStartStoredWh = 0.0f;
    float chargeCycleInputWh = 0.0f;

    bool configChanged = false;

    void integrateEnergy(const PowerSample &sample, float dtHours);
    void updatePowerState();
    void updateSoc();
    void updateEstimatedTime(uint32_t nowMs);
    void updateAveragedPower(uint32_t nowMs);
    float quantizeEtaHours(float hours) const;
    void resetEtaAverage();

    void updateChargeCycle(PowerState previousState);
    void updateFullChargeDetection(uint32_t nowMs);
    void completeAutomaticFullCharge(uint32_t nowMs);
    void applyChargeEfficiencyCalibration();

    bool isFullChargeCondition() const;
    bool isLearningFinished() const;

    void startLearning(uint32_t timeMs);
    void finishLearning(uint32_t timeMs);
    void applyCapacityCorrection(float measuredCapacityWh);
};
