#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#include "Config.h"
#include "Types.h"
#include "SmallOled128x32.h"

class DisplayManager {
public:
    bool begin(TwoWire *smallWire);

    void setSmallDisplayOn(bool on);
    void setMainDisplayOn(bool on);

    bool isSmallDisplayOn() const;
    bool isMainDisplayOn() const;

    void playPowerOnAnimation();
    void playPowerOffAnimation();

    void renderSmall(
        const BatteryState &battery,
        SystemState systemState,
        bool bluetoothEnabled,
        bool bluetoothConnected,
        uint8_t animationFrame
    );

    void renderMain(
        MainPage page,
        const BatteryState &battery,
        const BatteryConfig &batteryConfig,
        const UiConfig &uiConfig,
        bool bluetoothEnabled,
        bool bluetoothConnected
    );

private:
    Adafruit_SH1106G mainDisplay = Adafruit_SH1106G(
        Config::DISPLAY_MAIN_WIDTH,
        Config::DISPLAY_MAIN_HEIGHT,
        &Wire,
        -1
    );

    SmallOled128x32 smallDisplay;

    bool smallDisplayOn = false;
    bool mainDisplayOn = false;

    bool mainOk = false;
    bool smallOk = false;

    void drawSmallBatteryBar(float socPercent, PowerState powerState, uint8_t animationFrame);
    void drawSmallBluetoothIcon(int x, int y);

    void renderBatPowerPage(const BatteryState &battery);
    void renderCapacityLearnPage(const BatteryState &battery, const BatteryConfig &batteryConfig);
    void renderSettingsPage(const BatteryConfig &batteryConfig, const UiConfig &uiConfig);
    void renderBluetoothPage(bool bluetoothEnabled, bool bluetoothConnected);

    void drawMainProgressBar(int x, int y, int w, int h, float percent);
};
