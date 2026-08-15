#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "Types.h"

class SettingsStorage {
public:
    void begin();
    PersistentData load();
    void save(const PersistentData &data);

    CoolingConfig loadCoolingConfig();
    void saveCoolingConfig(const CoolingConfig &config);

    PowerMeasurementConfig loadPowerMeasurementConfig();
    void savePowerMeasurementConfig(const PowerMeasurementConfig &config);

    uint32_t loadSystemIdleTimeoutSec(uint32_t defaultValue);
    void saveSystemIdleTimeoutSec(uint32_t value);

    void reset();

private:
    Preferences prefs;
};

class StateSaver {
public:
    void begin(const SaveConfig &config, float initialStoredWh);
    bool shouldSave(float currentStoredWh, uint32_t nowMs);
    void markSaved(float currentStoredWh, uint32_t nowMs);
    void requestForceSave();

private:
    SaveConfig config;
    uint32_t lastSaveMs = 0;
    float lastSavedStoredWh = 0.0f;
    bool forceSaveRequested = false;
};
