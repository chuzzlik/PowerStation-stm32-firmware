#include "AppController.h"
#include "Config.h"
#include "Utils.h"
#include <math.h>
#include <esp_system.h>

void AppController::begin() {
    // Максимально рано оставляем выключенными силовой выход и вентилятор.
    pinMode(Config::PIN_FAN_PWM, OUTPUT);
    digitalWrite(Config::PIN_FAN_PWM, LOW);

    pinMode(Config::PIN_MOSFET_OUTPUT, OUTPUT);
    digitalWrite(
        Config::PIN_MOSFET_OUTPUT,
        Config::MOSFET_ACTIVE_HIGH ? LOW : HIGH
    );

    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("Powerbank boot");
    Serial.print("Firmware: ");
    Serial.println(Config::FIRMWARE_VERSION);

    Wire.begin(Config::DISPLAY_MAIN_SDA, Config::DISPLAY_MAIN_SCL);
    smallDisplayWire.begin(Config::DISPLAY_SMALL_SDA, Config::DISPLAY_SMALL_SCL);
    smallDisplayWire.setClock(100000);

    scanI2C(Wire, "Main OLED Wire");
    scanI2C(smallDisplayWire, "Small OLED Wire1");

    storage.begin();
    persistentData = storage.load();

    const esp_reset_reason_t resetReason = esp_reset_reason();
    const bool restoreAfterUnexpectedReset =
        persistentData.systemWasOn
        && resetReason != ESP_RST_POWERON
        && resetReason != ESP_RST_EXT;

    Serial.print("Reset reason: ");
    Serial.println(static_cast<int>(resetReason));
    Serial.print("Restore power state: ");
    Serial.println(restoreAfterUnexpectedReset ? "ON" : "OFF");

    inaOk = powerSensor.begin(
        Config::SHUNT_SDA,
        Config::SHUNT_SCL,
        Config::INA226_ADDRESS,
        Config::SHUNT_OHMS,
        Config::INVERT_CURRENT
    );

    Serial.print("INA226: ");
    Serial.println(inaOk ? "OK" : "NOT FOUND");

    batteryMeter.begin(
        persistentData.batteryConfig,
        persistentData.currentStoredWh
    );

    BatteryServiceInfo serviceInfo;
    serviceInfo.learnedCycles = persistentData.learnedCycles;
    serviceInfo.lastMeasuredCapacityWh = persistentData.lastMeasuredCapacityWh;
    serviceInfo.lastCorrectionWh = persistentData.lastCorrectionWh;
    serviceInfo.lastCorrectionPercent = persistentData.lastCorrectionPercent;
    serviceInfo.totalDischargeWh = persistentData.totalDischargeWh;
    serviceInfo.totalChargeWh = persistentData.totalChargeWh;
    batteryMeter.setServiceInfo(serviceInfo);

    stateSaver.begin(saveConfig, persistentData.currentStoredWh);

    mosfetOutput.begin(Config::PIN_MOSFET_OUTPUT);
    buttons.begin();
    statusLed.begin(Config::PIN_POWER_LED);
    cooling.begin();

    xTaskCreatePinnedToCore(
        AppController::ledTask,
        "StatusLed",
        2048,
        this,
        2,
        &ledTaskHandle,
        0
    );

    bool displaysOk = displays.begin(&smallDisplayWire);
    Serial.print("Displays: ");
    Serial.println(displaysOk ? "OK" : "ERROR");

    ble.begin(this);

    uint32_t now = millis();

    if (restoreAfterUnexpectedReset) {
        systemState = SystemState::On;
        mosfetOutput.enable();
        batteryMeter.clearOutputDisabledByProtection();
        setLastEvent("AUTO RESTORE");
    } else {
        systemState = SystemState::Off;
        mosfetOutput.disable();
    }

    smallDisplayOn = true;
    mainDisplayOn = false;

    smallDisplayPreviewUntilMs = restoreAfterUnexpectedReset
        ? 0
        : now + Config::SMALL_SCREEN_OFF_PREVIEW_MS;

    lastSmallDisplayActivityMs = now;
    lastMainDisplayActivityMs = now;
    mainPage = MainPage::BatPower;

    displays.setSmallDisplayOn(true);
    displays.setMainDisplayOn(false);

    if (!restoreAfterUnexpectedReset) {
        setLastEvent("BOOT");
    }

    displays.renderSmall(
        batteryMeter.getState(),
        systemState,
        ble.isEnabled(),
        ble.isConnected(),
        animationFrame
    );
}

void AppController::ledTask(void *param) {
    AppController *app = static_cast<AppController *>(param);

    for (;;) {
        app->statusLed.update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void AppController::update() {
    cooling.update();
    readButtons();
    readPowerData();

    updatePowerState();
    updateSystemProtection();
    updateBluetooth();

    updateSmallDisplayState();
    updateMainDisplayState();

    updateLed();
    renderDisplaysIfNeeded();
    updateSaving();
}

void AppController::scanI2C(TwoWire &wire, const char *name) {
    Serial.print("I2C scan: ");
    Serial.println(name);

    bool found = false;

    for (uint8_t address = 1; address < 127; address++) {
        wire.beginTransmission(address);

        if (wire.endTransmission() == 0) {
            found = true;
            Serial.print("Found: 0x");

            if (address < 16) {
                Serial.print("0");
            }

            Serial.println(address, HEX);
        }
    }

    if (!found) {
        Serial.println("No devices found");
    }

    Serial.println("Scan done");
}

void AppController::readButtons() {
    ButtonEvent event = buttons.update();

    if (event != ButtonEvent::None) {
        handleButtonEvent(event);
    }
}

void AppController::readPowerData() {
    uint32_t now = millis();

    if (now - lastSensorUpdateMs < Config::SENSOR_REFRESH_MS) {
        return;
    }

    lastSensorUpdateMs = now;

    if (!inaOk) {
        inaOk = powerSensor.begin(
            Config::SHUNT_SDA,
            Config::SHUNT_SCL,
            Config::INA226_ADDRESS,
            Config::SHUNT_OHMS,
            Config::INVERT_CURRENT
        );

        if (!inaOk) {
            return;
        }

        setLastEvent("INA OK");
        wakeMainDisplay(MainPage::BatPower, true);
    }

    PowerSample sample = powerSensor.read();
    batteryMeter.update(sample);

    if (batteryMeter.consumeConfigChanged()) {
        requestForceSave();
        setLastEvent("CHARGE EFF AUTO");
    }
}

void AppController::updatePowerState() {
    uint32_t now = millis();
    previousPowerState = powerState;
    powerState = batteryMeter.getState().powerState;

    if (
        powerState != previousPowerState
        && systemState == SystemState::On
        && previousPowerState == PowerState::Idle
        && (powerState == PowerState::Charge || powerState == PowerState::Discharge)
    ) {
        wakeSmallDisplay();
    }

    const bool chargeInputDetectedNow =
        batteryMeter.getState().powerW >= Config::AUTO_POWER_ON_MIN_CHARGE_W;

    if (chargeInputDetectedNow != chargeInputDetected) {
        chargeInputDetected = chargeInputDetectedNow;

        if (systemState == SystemState::Off && chargeInputDetected) {
            autoPowerOnPending = true;
            autoPowerOnStartedMs = now;
        } else if (!chargeInputDetected) {
            autoPowerOnPending = false;
        }
    }

    if (
        systemState == SystemState::Off
        && autoPowerOnPending
        && chargeInputDetected
        && now - autoPowerOnStartedMs >= Config::AUTO_POWER_ON_CHARGE_DELAY_MS
    ) {
        autoPowerOnPending = false;
        powerSystemOn("AUTO CHARGE");
    }
}

void AppController::updateSystemProtection() {
    if (systemState != SystemState::On) {
        return;
    }

    BatteryState battery = batteryMeter.getState();
    BatteryConfig config = batteryMeter.getConfig();

    if (battery.powerState == PowerState::Charge) {
        return;
    }

    bool lowBySoc = battery.socPercent <= config.criticalSocPercent;
    bool lowByVoltage = battery.voltageV > 1.0f && battery.voltageV <= config.lowCutVoltageV;
    bool overPower = fabsf(battery.powerW) >= config.powerLimitW;

    if (overPower) {
        shutdownByProtection("OVERLOAD");
        return;
    }

    if (lowBySoc || lowByVoltage) {
        shutdownByProtection("LOW BAT");
    }
}

void AppController::updateSmallDisplayState() {
    uint32_t now = millis();
    uint32_t timeoutMs = persistentData.uiConfig.smallScreenTimeoutSec * 1000UL;

    if (systemState == SystemState::Off) {
        if (smallDisplayPreviewUntilMs > 0) {
            if (smallDisplayOn && static_cast<int32_t>(now - smallDisplayPreviewUntilMs) >= 0) {
                smallDisplayPreviewUntilMs = 0;
                sleepSmallDisplay();
            }
            return;
        }

        if (smallDisplayOn && now - lastSmallDisplayActivityMs > timeoutMs) {
            sleepSmallDisplay();
        }
        return;
    }

    if (powerState == PowerState::Charge || powerState == PowerState::Discharge) {
        smallDisplayPreviewUntilMs = 0;
        wakeSmallDisplay();
        lastSmallDisplayActivityMs = now;
        return;
    }

    if (powerState == PowerState::Idle && smallDisplayOn && now - lastSmallDisplayActivityMs > timeoutMs) {
        sleepSmallDisplay();
    }
}

void AppController::updateMainDisplayState() {
    if (!mainDisplayOn) {
        return;
    }

    uint32_t timeoutMs = persistentData.uiConfig.mainScreenTimeoutSec * 1000UL;

    if (millis() - lastMainDisplayActivityMs > timeoutMs) {
        sleepMainDisplay();
    }
}

void AppController::renderDisplaysIfNeeded() {
    uint32_t now = millis();

    if (now - lastAnimationFrameMs >= Config::ANIMATION_REFRESH_MS) {
        lastAnimationFrameMs = now;
        animationFrame++;
    }

    if (now - lastDisplayRenderMs < Config::DISPLAY_REFRESH_MS) {
        return;
    }

    lastDisplayRenderMs = now;

    BatteryState battery = batteryMeter.getState();
    BatteryConfig config = batteryMeter.getConfig();

    displays.renderSmall(
        battery,
        systemState,
        ble.isEnabled(),
        ble.isConnected(),
        animationFrame
    );

    displays.renderMain(
        mainPage,
        battery,
        config,
        persistentData.uiConfig,
        cooling.getState(),
        ble.isEnabled(),
        ble.isConnected()
    );
}

void AppController::updateBluetooth() {
    ble.update();

    uint32_t now = millis();
    bluetoothConnected = ble.isConnected();

    if (bluetoothConnected != previousBluetoothConnected) {
        previousBluetoothConnected = bluetoothConnected;

        if (ble.isEnabled()) {
            setLastEvent(bluetoothConnected ? "BLE CONN" : "BLE DISC");
            wakeMainDisplay(MainPage::Bluetooth, true);
        }

        bleWaitingStartedMs = bluetoothConnected ? 0 : now;
    }

    if (!ble.isEnabled()) {
        bleWaitingStartedMs = 0;
        return;
    }

    if (bluetoothConnected) {
        bleWaitingStartedMs = 0;
        return;
    }

    if (bleWaitingStartedMs == 0) {
        bleWaitingStartedMs = now;
    }

    if (now - bleWaitingStartedMs < Config::BLE_WAITING_TIMEOUT_MS) {
        return;
    }

    ble.disable();
    previousBluetoothConnected = false;
    bleWaitingStartedMs = 0;
    setLastEvent("BLE TIMEOUT");

    if (mainPage == MainPage::Bluetooth) {
        mainPage = MainPage::BatPower;
    }

    if (mainDisplayOn) {
        wakeMainDisplay(mainPage, true);
    }
}

void AppController::updateLed() {
    statusLed.setMode(resolveLedMode());
}

void AppController::updateSaving() {
    BatteryState battery = batteryMeter.getState();
    uint32_t now = millis();

    if (!stateSaver.shouldSave(battery.currentStoredWh, now)) {
        return;
    }

    savePersistentData();
    stateSaver.markSaved(battery.currentStoredWh, now);
}

void AppController::handleButtonEvent(ButtonEvent event) {
    switch (event) {
        case ButtonEvent::PowerShort:
            handlePowerShort();
            break;

        case ButtonEvent::PowerLong:
            handlePowerLong();
            break;

        case ButtonEvent::ScreenShort:
            handleScreenShort();
            break;

        case ButtonEvent::ScreenLong:
            handleScreenLong();
            break;

        case ButtonEvent::None:
            break;
    }
}

void AppController::handlePowerShort() {
    if (systemState == SystemState::On) {
        smallDisplayPreviewUntilMs = 0;
        wakeSmallDisplay();
        wakeMainDisplay(MainPage::BatPower, true);
        return;
    }

    wakeSmallDisplay();
    smallDisplayPreviewUntilMs = millis() + Config::SMALL_SCREEN_OFF_PREVIEW_MS;
}

void AppController::handlePowerLong() {
    if (systemState == SystemState::Off) {
        powerSystemOn();
    } else {
        powerSystemOff();
    }
}

void AppController::handleScreenShort() {
    if (systemState == SystemState::Off) {
        return;
    }

    if (!mainDisplayOn) {
        wakeMainDisplayCurrentPage();
        return;
    }

    nextMainPage();
    wakeMainDisplayCurrentPage();
}

void AppController::handleScreenLong() {
    if (systemState == SystemState::Off) {
        return;
    }

    if (!ble.isEnabled()) {
        ble.enable();
        previousBluetoothConnected = false;
        bleWaitingStartedMs = millis();

        setLastEvent("BLE ON");
        wakeSmallDisplay();
        wakeMainDisplay(MainPage::Bluetooth, true);
        return;
    }

    ble.disable();
    previousBluetoothConnected = false;
    bleWaitingStartedMs = 0;
    setLastEvent("BLE OFF");

    if (mainPage == MainPage::Bluetooth) {
        mainPage = MainPage::BatPower;
    }

    wakeSmallDisplay();

    if (mainDisplayOn) {
        wakeMainDisplay(mainPage, true);
    }
}

void AppController::powerSystemOn(const char *eventName) {
    smallDisplayPreviewUntilMs = 0;
    wakeSmallDisplay();
    displays.playPowerOnAnimation();

    mosfetOutput.enable();
    batteryMeter.clearOutputDisabledByProtection();
    systemState = SystemState::On;
    persistentData.systemWasOn = true;

    requestForceSave();
    updateSaving();

    setLastEvent(eventName);
    wakeMainDisplay(MainPage::BatPower, true);
}

void AppController::powerSystemOff() {
    smallDisplayPreviewUntilMs = 0;
    autoPowerOnPending = false;

    displays.playPowerOffAnimation();

    mosfetOutput.disable();
    systemState = SystemState::Off;
    persistentData.systemWasOn = false;

    setLastEvent("POWER OFF");

    requestForceSave();
    updateSaving();

    sleepSmallDisplay();

    mainDisplayOn = false;
    displays.setMainDisplayOn(false);

    ble.disable();
    previousBluetoothConnected = false;
    bleWaitingStartedMs = 0;
}

void AppController::shutdownByProtection(const String &eventName) {
    smallDisplayPreviewUntilMs = 0;
    autoPowerOnPending = false;
    batteryMeter.markOutputDisabledByProtection();

    mosfetOutput.disable();
    systemState = SystemState::Off;
    persistentData.systemWasOn = false;

    requestForceSave();
    updateSaving();

    setLastEvent(eventName);

    wakeSmallDisplay();
    wakeMainDisplay(MainPage::BatPower, true);
}

void AppController::wakeSmallDisplay() {
    smallDisplayOn = true;
    lastSmallDisplayActivityMs = millis();
    displays.setSmallDisplayOn(true);
}

void AppController::wakeMainDisplay(MainPage page, bool resetTimer) {
    if (page == MainPage::Bluetooth && !isBluetoothPageAvailable()) {
        page = MainPage::BatPower;
    }

    mainPage = page;
    mainDisplayOn = true;

    if (resetTimer) {
        lastMainDisplayActivityMs = millis();
    }

    displays.setMainDisplayOn(true);
}

void AppController::wakeMainDisplayCurrentPage() {
    wakeMainDisplay(mainPage, true);
}

void AppController::sleepSmallDisplay() {
    smallDisplayOn = false;
    displays.setSmallDisplayOn(false);
}

void AppController::sleepMainDisplay() {
    mainDisplayOn = false;
    displays.setMainDisplayOn(false);
}

void AppController::nextMainPage() {
    uint8_t pageCount = getMainPageCount();
    uint8_t next = static_cast<uint8_t>(mainPage) + 1;

    if (next >= pageCount) {
        next = 0;
    }

    mainPage = static_cast<MainPage>(next);
}

uint8_t AppController::getMainPageCount() const {
    return isBluetoothPageAvailable() ? 4 : 3;
}

bool AppController::isBluetoothPageAvailable() const {
    return ble.isEnabled() || ble.isConnected();
}

void AppController::setLastEvent(const String &event) {
    Serial.print("Event: ");
    Serial.println(event);
}

void AppController::savePersistentData() {
    BatteryState battery = batteryMeter.getState();
    BatteryConfig batteryConfig = batteryMeter.getConfig();
    BatteryServiceInfo service = batteryMeter.getServiceInfo();

    persistentData.batteryConfig = batteryConfig;
    persistentData.currentStoredWh = battery.currentStoredWh;

    persistentData.learnedCycles = service.learnedCycles;
    persistentData.lastMeasuredCapacityWh = service.lastMeasuredCapacityWh;
    persistentData.lastCorrectionWh = service.lastCorrectionWh;
    persistentData.lastCorrectionPercent = service.lastCorrectionPercent;
    persistentData.totalDischargeWh = service.totalDischargeWh;
    persistentData.totalChargeWh = service.totalChargeWh;

    storage.save(persistentData);

    Serial.println("Saved");
}

void AppController::requestForceSave() {
    stateSaver.requestForceSave();
}

StatusLedMode AppController::resolveLedMode() const {
    if (systemState == SystemState::Off) {
        return StatusLedMode::Off;
    }

    BatteryState battery = batteryMeter.getState();
    BatteryConfig config = batteryMeter.getConfig();

    if (battery.powerState == PowerState::Charge) {
        return StatusLedMode::ChargeBlink;
    }

    if (battery.outputDisabledByProtection || battery.socPercent <= config.lowSocPercent) {
        return StatusLedMode::LowBatteryBlink;
    }

    switch (battery.powerState) {
        case PowerState::Charge:
            return StatusLedMode::ChargeBlink;

        case PowerState::Discharge:
            return StatusLedMode::DischargeSolid;

        case PowerState::Idle:
            return StatusLedMode::IdleBreathing;
    }

    return StatusLedMode::Off;
}

void AppController::sendBleResult(const String &command, bool ok, const String &error) {
    String json = "{\"type\":\"result\",\"command\":\"" + command + "\",\"ok\":";
    json += ok ? "true" : "false";

    if (!ok) {
        json += ",\"error\":\"" + error + "\"";
    }

    json += "}";
    ble.notifyStatus(json);
}

void AppController::handleBleCommand(const String &command) {
    String cmd = command;
    cmd.trim();

    if (cmd.length() == 0) {
        sendBleResult("", false, "empty_command");
        return;
    }

    Serial.print("BLE CMD: ");
    Serial.println(cmd);

    if (cmd == "status" || cmd == "get status") {
        ble.notifyStatus(makeStatusJson());
        return;
    }

    if (cmd == "settings" || cmd == "get settings") {
        ble.notifySettings(makeSettingsJson());
        return;
    }

    if (cmd == "all" || cmd == "get all") {
        ble.notifySettings(makeSettingsJson());
        ble.notifyStatus(makeStatusJson());
        return;
    }

    if (cmd == "save") {
        requestForceSave();
        updateSaving();
        sendBleResult(cmd, true);
        return;
    }

    if (cmd == "resetLearning") {
        batteryMeter.resetLearning();
        requestForceSave();
        updateSaving();
        wakeMainDisplay(MainPage::CapacityLearn, true);
        sendBleResult(cmd, true);
        return;
    }

    if (cmd == "markFull") {
        batteryMeter.markFullCharge();
        requestForceSave();
        updateSaving();
        wakeMainDisplay(MainPage::CapacityLearn, true);
        sendBleResult(cmd, true);
        return;
    }

    if (cmd.startsWith("set ")) {
        String error;
        bool ok = handleSetCommand(cmd.substring(4), error);

        if (ok) {
            requestForceSave();
            updateSaving();
            wakeMainDisplay(MainPage::Settings, true);
            ble.notifySettings(makeSettingsJson());
        }

        sendBleResult(cmd, ok, error);
        return;
    }

    sendBleResult(cmd, false, "unknown_command");
}

String AppController::makeStatusJson() {
    BatteryState battery = batteryMeter.getState();
    BatteryServiceInfo service = batteryMeter.getServiceInfo();
    CoolingState thermal = cooling.getState();

    String json;
    json.reserve(500);
    json = "{\"type\":\"status\",";
    json += "\"apiVersion\":6,";
    json += "\"firmwareVersion\":\"" + String(Config::FIRMWARE_VERSION) + "\",";
    json += "\"systemState\":\"";
    json += systemState == SystemState::On ? "ON" : "OFF";
    json += "\",";
    json += "\"powerState\":\"" + String(powerStateToText(battery.powerState)) + "\",";
    json += "\"socPercent\":" + String(battery.socPercent, 2) + ",";
    json += "\"voltageV\":" + String(battery.voltageV, 3) + ",";
    json += "\"currentA\":" + String(battery.currentA, 3) + ",";
    json += "\"powerW\":" + String(battery.powerW, 3) + ",";
    json += "\"averagedPowerW\":" + String(battery.averagedPowerW, 3) + ",";
    json += "\"currentStoredWh\":" + String(battery.currentStoredWh, 3) + ",";
    json += "\"learnedCapacityWh\":" + String(battery.learnedCapacityWh, 3) + ",";
    json += "\"estimatedTimeHours\":" + String(battery.estimatedTimeHours, 3) + ",";
    json += "\"learningActive\":" + String(battery.learningActive ? "true" : "false") + ",";
    json += "\"learningDischargeWh\":" + String(battery.learningDischargeWh, 3) + ",";
    json += "\"learnedCycles\":" + String(service.learnedCycles) + ",";
    json += "\"tempPowerC\":";
    json += thermal.powerSensorValid ? String(thermal.powerTemperatureC, 2) : "null";
    json += ",\"tempAirC\":";
    json += thermal.airSensorValid ? String(thermal.airTemperatureC, 2) : "null";
    json += ",\"fanPercent\":" + String(thermal.fanPercent) + ",";
    json += "\"thermalFault\":" + String(thermal.fault ? "true" : "false") + ",";
    json += "\"mosfetEnabled\":" + String(mosfetOutput.isEnabled() ? "true" : "false") + ",";
    json += "\"bluetoothEnabled\":" + String(ble.isEnabled() ? "true" : "false") + ",";
    json += "\"bluetoothConnected\":" + String(ble.isConnected() ? "true" : "false");
    json += "}";

    return json;
}

String AppController::makeSettingsJson() {
    BatteryConfig config = batteryMeter.getConfig();

    String json;
    json.reserve(500);
    json = "{\"type\":\"settings\",";
    json += "\"apiVersion\":6,";
    json += "\"nominalCapacityWh\":" + String(config.nominalCapacityWh, 3) + ",";
    json += "\"lowCutVoltageV\":" + String(config.lowCutVoltageV, 3) + ",";
    json += "\"fullVoltageV\":" + String(config.fullVoltageV, 3) + ",";
    json += "\"fullCurrentA\":" + String(config.fullCurrentA, 3) + ",";
    json += "\"chargeEfficiency\":" + String(config.chargeEfficiency, 3) + ",";
    json += "\"chargeEfficiencyAuto\":true,";
    json += "\"lowSocPercent\":" + String(config.lowSocPercent, 2) + ",";
    json += "\"criticalSocPercent\":" + String(config.criticalSocPercent, 2) + ",";
    json += "\"learningEndVoltageV\":" + String(config.learningEndVoltageV, 3) + ",";
    json += "\"learningMinDischargeWh\":" + String(config.learningMinDischargeWh, 2) + ",";
    json += "\"learningCorrectionAlpha\":" + String(config.learningCorrectionAlpha, 3) + ",";
    json += "\"powerLimitW\":" + String(config.powerLimitW, 2) + ",";
    json += "\"etaAveragingSeconds\":" + String(config.etaAveragingSeconds, 1) + ",";
    json += "\"etaIdleHoldSeconds\":" + String(config.etaIdleHoldSeconds, 1) + ",";
    json += "\"etaMinPowerW\":" + String(config.etaMinPowerW, 2) + ",";
    json += "\"etaMaxHours\":" + String(config.etaMaxHours, 2) + ",";
    json += "\"smallScreenTimeoutSec\":" + String(persistentData.uiConfig.smallScreenTimeoutSec) + ",";
    json += "\"mainScreenTimeoutSec\":" + String(persistentData.uiConfig.mainScreenTimeoutSec);
    json += "}";

    return json;
}

bool AppController::handleSetCommand(const String &expression, String &error) {
    int eqPos = expression.indexOf('=');

    if (eqPos < 1) {
        error = "expected_key_equals_value";
        return false;
    }

    String key = expression.substring(0, eqPos);
    String value = expression.substring(eqPos + 1);
    key.trim();
    value.trim();

    if (value.length() == 0) {
        error = "empty_value";
        return false;
    }

    char *endPtr = nullptr;
    float floatValue = strtof(value.c_str(), &endPtr);
    if (endPtr == value.c_str() || *endPtr != '\0' || !isfinite(floatValue)) {
        error = "invalid_number";
        return false;
    }

    int intValue = static_cast<int>(floatValue);
    BatteryConfig c = batteryMeter.getConfig();

    if (key == "nominalCapacityWh") {
        c.nominalCapacityWh = clampFloat(floatValue, 1.0f, 5000.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "learnedCapacityWh") {
        batteryMeter.setLearnedCapacityWh(floatValue);
        return true;
    }
    if (key == "currentStoredWh") {
        batteryMeter.setCurrentStoredWh(floatValue);
        return true;
    }
    if (key == "remainingPercent") {
        batteryMeter.setRemainingPercent(floatValue);
        return true;
    }
    if (key == "lowCutVoltageV") {
        c.lowCutVoltageV = clampFloat(floatValue, 1.0f, 60.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "fullVoltageV") {
        c.fullVoltageV = clampFloat(floatValue, 1.0f, 60.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "fullCurrentA") {
        c.fullCurrentA = clampFloat(floatValue, 0.01f, 20.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "lowSocPercent") {
        c.lowSocPercent = clampFloat(floatValue, 0.0f, 100.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "criticalSocPercent") {
        c.criticalSocPercent = clampFloat(floatValue, 0.0f, 100.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "learningEndVoltageV") {
        c.learningEndVoltageV = clampFloat(floatValue, 1.0f, 60.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "learningMinDischargeWh") {
        c.learningMinDischargeWh = clampFloat(floatValue, 1.0f, 5000.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "learningCorrectionAlpha") {
        c.learningCorrectionAlpha = clampFloat(floatValue, 0.01f, 1.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "powerLimitW") {
        c.powerLimitW = clampFloat(floatValue, 5.0f, 2000.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "etaAveragingSeconds") {
        c.etaAveragingSeconds = clampFloat(floatValue, 5.0f, 300.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "etaIdleHoldSeconds") {
        c.etaIdleHoldSeconds = clampFloat(floatValue, 0.0f, 120.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "etaMinPowerW") {
        c.etaMinPowerW = clampFloat(floatValue, Config::POWER_DEADZONE_W, 100.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "etaMaxHours") {
        c.etaMaxHours = clampFloat(floatValue, 1.0f, 1000.0f);
        batteryMeter.setConfig(c);
        return true;
    }
    if (key == "smallScreenTimeoutSec") {
        persistentData.uiConfig.smallScreenTimeoutSec = static_cast<uint16_t>(constrain(intValue, 5, 3600));
        return true;
    }
    if (key == "mainScreenTimeoutSec") {
        persistentData.uiConfig.mainScreenTimeoutSec = static_cast<uint16_t>(constrain(intValue, 5, 3600));
        return true;
    }

    error = key == "chargeEfficiency"
        ? "read_only_auto_setting"
        : "unknown_setting";
    return false;
}
