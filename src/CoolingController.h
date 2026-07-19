#pragma once

#include <Arduino.h>

struct CoolingState {
    float powerTemperatureC = NAN;
    float airTemperatureC = NAN;
    uint8_t fanPercent = 0;
    bool powerSensorValid = false;
    bool airSensorValid = false;
    bool fanOn = false;
    bool fault = false;
};

class CoolingController {
public:
    void begin();
    void update();
    CoolingState getState() const;

private:
    CoolingState state;

    bool powerTemperatureReady = false;
    bool airTemperatureReady = false;
    bool fanRequested = false;
    bool startupBoostActive = false;

    uint32_t lastSensorReadMs = 0;
    uint32_t lastFanUpdateMs = 0;
    uint32_t startupBoostUntilMs = 0;

    float targetFanPercent = 0.0f;
    float actualFanPercent = 0.0f;

    bool readTemperature(uint8_t pin, float &temperatureC) const;
    float smoothTemperature(float previousC, float currentC, float dtSeconds, bool &ready) const;
    float calculateTargetFanPercent(float temperatureC) const;
    void updateFan(uint32_t nowMs);
    void writeFanPercent(float percent);
};
