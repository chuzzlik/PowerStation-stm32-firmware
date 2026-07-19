#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "Types.h"
#include "Ina226PowerSensor.h"
#include "BatteryMeter.h"
#include "SettingsStorage.h"
#include "MosfetOutput.h"
#include "ButtonInput.h"
#include "StatusLed.h"
#include "DisplayManager.h"
#include "BlePowerService.h"

class AppController {
    static void ledTask(void *param);
    TaskHandle_t ledTaskHandle = nullptr;
public:
    void begin();
    void update();

    void handleBleCommand(const String &command);
    String makeStatusJson();
    String makeSettingsJson();

private:
    TwoWire smallDisplayWire = TwoWire(1);

    Ina226PowerSensor powerSensor;
    BatteryMeter batteryMeter;
    SettingsStorage storage;
    StateSaver stateSaver;
    MosfetOutput mosfetOutput;
    ButtonInput buttons;
    StatusLed statusLed;
    DisplayManager displays;
    BlePowerService ble;

    PersistentData persistentData;
    SaveConfig saveConfig;

    SystemState systemState = SystemState::Off;
    PowerState powerState = PowerState::Idle;
    PowerState previousPowerState = PowerState::Idle;

    bool inaOk = false;

    bool smallDisplayOn = false;
    bool mainDisplayOn = false;

    bool bluetoothConnected = false;
    bool previousBluetoothConnected = false;

    MainPage mainPage = MainPage::BatPower;

    uint8_t animationFrame = 0;

    uint32_t lastSensorUpdateMs = 0;
    uint32_t lastSmallDisplayActivityMs = 0;
    uint32_t lastMainDisplayActivityMs = 0;
    uint32_t lastAnimationFrameMs = 0;
    uint32_t lastDisplayRenderMs = 0;
    uint32_t smallDisplayPreviewUntilMs = 0;
    bool chargeInputDetected = false;
    bool autoPowerOnPending = false;
    uint32_t autoPowerOnStartedMs = 0;

    void scanI2C(TwoWire &wire, const char *name);

    void readButtons();
    void readPowerData();

    void updatePowerState();
    void updateSystemProtection();

    void updateSmallDisplayState();
    void updateMainDisplayState();

    void renderDisplaysIfNeeded();
    void updateBluetooth();
    void updateLed();
    void updateSaving();

    void handleButtonEvent(ButtonEvent event);

    void handlePowerShort();
    void handlePowerLong();
    void handleScreenShort();
    void handleScreenLong();

    void powerSystemOn(const char *eventName = "POWER ON");
    void powerSystemOff();
    void shutdownByProtection(const String &eventName);

    void wakeSmallDisplay();
    void wakeMainDisplay(MainPage page, bool resetTimer);
    void wakeMainDisplayCurrentPage();
    void sleepSmallDisplay();
    void sleepMainDisplay();

    void nextMainPage();
    uint8_t getMainPageCount() const;
    bool isBluetoothPageAvailable() const;

    void setLastEvent(const String &event);
    void savePersistentData();
    void requestForceSave();

    StatusLedMode resolveLedMode() const;

    bool handleSetCommand(const String &expression, String &error);
    void sendBleResult(const String &command, bool ok, const String &error = "");
};
