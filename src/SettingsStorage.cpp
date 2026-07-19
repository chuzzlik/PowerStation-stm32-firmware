#include "SettingsStorage.h"
#include <math.h>

void SettingsStorage::begin() {
    prefs.begin("powerbank", false);
}

PersistentData SettingsStorage::load() {
    PersistentData data;
    size_t size = prefs.getBytesLength("data");

    if (size != sizeof(PersistentData)) {
        return data;
    }

    prefs.getBytes("data", &data, sizeof(PersistentData));

    if (data.magic != 0x50425733 || data.version != 7) {
        return PersistentData();
    }

    return data;
}

void SettingsStorage::save(const PersistentData &data) {
    prefs.putBytes("data", &data, sizeof(PersistentData));
}

uint32_t SettingsStorage::loadSystemIdleTimeoutSec(uint32_t defaultValue) {
    return prefs.getUInt("sysIdleSec", defaultValue);
}

void SettingsStorage::saveSystemIdleTimeoutSec(uint32_t value) {
    prefs.putUInt("sysIdleSec", value);
}

void SettingsStorage::reset() {
    prefs.clear();
}

void StateSaver::begin(const SaveConfig &config, float initialStoredWh) {
    this->config = config;
    lastSavedStoredWh = initialStoredWh;
    lastSaveMs = millis();
}

bool StateSaver::shouldSave(float currentStoredWh, uint32_t nowMs) {
    if (forceSaveRequested) {
        return true;
    }

    if (nowMs - lastSaveMs < config.intervalMs) {
        return false;
    }

    if (fabsf(currentStoredWh - lastSavedStoredWh) < config.minStoredWhDelta) {
        return false;
    }

    return true;
}

void StateSaver::markSaved(float currentStoredWh, uint32_t nowMs) {
    lastSavedStoredWh = currentStoredWh;
    lastSaveMs = nowMs;
    forceSaveRequested = false;
}

void StateSaver::requestForceSave() {
    forceSaveRequested = true;
}
