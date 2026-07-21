#pragma once

#include <Arduino.h>

enum class PowerState : uint8_t {
    Idle,
    Charge,
    Discharge
};

enum class SystemState : uint8_t {
    Off,
    On
};

enum class ButtonEvent : uint8_t {
    None,
    PowerShort,
    PowerLong,
    ScreenShort,
    ScreenLong
};

enum class StatusLedMode : uint8_t {
    Off,
    IdleBreathing,
    ChargeBlink,
    DischargeSolid,
    LowBatteryBlink
};

enum class MainPage : uint8_t {
    BatPower = 0,
    CapacityLearn = 1,
    Settings = 2,
    Bluetooth = 3
};

enum class LearnCycle : uint8_t {
    Idle,
    Charge,
    Discharge
};

enum class LearnStatus : uint8_t {
    Inactive,
    Active,
    Complete,
    Paused
};

struct PowerSample {
    float voltageV = 0.0f;
    float currentA = 0.0f;
    float powerW = 0.0f;
    uint32_t timeMs = 0;
};

struct BatteryConfig {
    float nominalCapacityWh = 450.0f;
    float learnedCapacityWh = 500.0f;

    float lowCutVoltageV = 11.0f;
    float fullVoltageV = 14.8f;
    float fullCurrentA = 0.2f;

    float chargeEfficiency = 0.95f;
    float lowSocPercent = 15.0f;
    float criticalSocPercent = 3.0f;

    float learningEndVoltageV = 10.4f;
    float learningMinDischargeWh = 50.0f;
    float learningCorrectionAlpha = 0.25f;

    float minAllowedCapacityWh = 150.0f;
    float maxAllowedCapacityWh = 500.0f;

    float powerLimitW = 100.0f;

    // Параметры устойчивого расчёта ETA.
    float etaAveragingSeconds = 30.0f;
    float etaIdleHoldSeconds = 10.0f;
    float etaMinPowerW = 3.0f;
    float etaMaxHours = 168.0f;
};

struct UiConfig {
    uint16_t smallScreenTimeoutSec = 30;
    uint16_t mainScreenTimeoutSec = 30;
};

struct CoolingConfig {
    float fanMinPercent = 40.0f;
    float fanStartPercent = 80.0f;
    uint32_t fanStartBoostMs = 1000;
    float fanOffTemperatureC = 38.0f;
    float fanOnTemperatureC = 42.0f;
    float fanFullTemperatureC = 60.0f;
};

struct BatteryState {
    PowerState powerState = PowerState::Idle;

    float voltageV = 0.0f;
    float currentA = 0.0f;
    float powerW = 0.0f;

    float currentStoredWh = 500.0f;
    float learnedCapacityWh = 500.0f;
    float socPercent = 100.0f;
    float estimatedTimeHours = -1.0f;
    float averagedPowerW = 0.0f;

    LearnCycle learnCycle = LearnCycle::Idle;
    LearnStatus learnStatus = LearnStatus::Inactive;

    bool learningActive = false;
    float learningDischargeWh = 0.0f;

    bool outputDisabledByProtection = false;
};

struct BatteryServiceInfo {
    uint32_t learnedCycles = 0;

    float lastMeasuredCapacityWh = 0.0f;
    float lastCorrectionWh = 0.0f;
    float lastCorrectionPercent = 0.0f;

    float totalDischargeWh = 0.0f;
    float totalChargeWh = 0.0f;

    uint32_t lastLearningStartedMs = 0;
    uint32_t lastLearningFinishedMs = 0;
};

struct PersistentData {
    uint32_t magic = 0x50425733; // PBW3
    uint32_t version = 7;

    BatteryConfig batteryConfig;
    UiConfig uiConfig;

    float currentStoredWh = 500.0f;

    uint32_t learnedCycles = 0;
    float lastMeasuredCapacityWh = 0.0f;
    float lastCorrectionWh = 0.0f;
    float lastCorrectionPercent = 0.0f;
    float totalDischargeWh = 0.0f;
    float totalChargeWh = 0.0f;

    // Последнее явно установленное пользователем состояние станции.
    // Используется только для восстановления после brownout/watchdog/software reset.
    bool systemWasOn = false;
};

struct SaveConfig {
    uint32_t intervalMs = 60000;
    float minStoredWhDelta = 0.5f;
};
