#include "DisplayManager.h"
#include "Utils.h"
#include <math.h>

bool DisplayManager::begin(TwoWire *smallWire) {
    mainOk = mainDisplay.begin(Config::DISPLAY_MAIN_ADDRESS, true);
    smallOk = smallDisplay.begin(smallWire, Config::DISPLAY_SMALL_ADDRESS);

    if (mainOk) {
        mainDisplay.clearDisplay();
        mainDisplay.setTextColor(SH110X_WHITE);
        mainDisplay.setTextSize(1);
        mainDisplay.display();
        mainDisplay.oled_command(SH110X_DISPLAYOFF);
        mainDisplayOn = false;
    }

    if (smallOk) {
        smallDisplay.setTextColor(1);
        smallDisplay.setTextSize(1);
        smallDisplay.clearDisplay();
        smallDisplay.display();
        smallDisplay.sleep();
        smallDisplayOn = false;
    }

    return mainOk && smallOk;
}

void DisplayManager::setSmallDisplayOn(bool on) {
    if (smallDisplayOn == on) {
        return;
    }

    smallDisplayOn = on;

    if (on) {
        smallDisplay.wake();
    } else {
        smallDisplay.sleep();
    }
}

void DisplayManager::setMainDisplayOn(bool on) {
    if (mainDisplayOn == on) {
        return;
    }

    mainDisplayOn = on;

    if (on) {
        mainDisplay.oled_command(SH110X_DISPLAYON);
    } else {
        mainDisplay.clearDisplay();
        mainDisplay.display();
        mainDisplay.oled_command(SH110X_DISPLAYOFF);
    }
}

bool DisplayManager::isSmallDisplayOn() const {
    return smallDisplayOn;
}

bool DisplayManager::isMainDisplayOn() const {
    return mainDisplayOn;
}

void DisplayManager::playPowerOnAnimation() {
    setMainDisplayOn(true);

    for (int i = 0; i <= 100; i += 10) {
        mainDisplay.clearDisplay();
        mainDisplay.setTextColor(SH110X_WHITE);
        mainDisplay.setTextSize(1);
        mainDisplay.setCursor(0, 0);
        mainDisplay.println("POWER ON");
        drawMainProgressBar(0, 24, 128, 10, i);
        mainDisplay.display();
        delay(35);
    }
}

void DisplayManager::playPowerOffAnimation() {
    setMainDisplayOn(true);

    for (int i = 100; i >= 0; i -= 10) {
        mainDisplay.clearDisplay();
        mainDisplay.setTextColor(SH110X_WHITE);
        mainDisplay.setTextSize(1);
        mainDisplay.setCursor(0, 0);
        mainDisplay.println("POWER OFF");
        drawMainProgressBar(0, 24, 128, 10, i);
        mainDisplay.display();
        delay(35);
    }
}

void DisplayManager::renderSmall(
    const BatteryState &battery,
    SystemState systemState,
    bool bluetoothEnabled,
    bool bluetoothConnected,
    uint8_t animationFrame
) {
    if (!smallDisplayOn) {
        return;
    }

    smallDisplay.clearDisplay();
    smallDisplay.setTextColor(1);

    const bool systemOff =
        systemState == SystemState::Off
        && battery.powerState == PowerState::Idle;

    smallDisplay.setTextSize(1);

    String topLeftText;
    String topRightText;

    if (!systemOff) {
        if (battery.powerState == PowerState::Charge) {
            topLeftText = formatDurationCompact(battery.estimatedTimeHours);
            topRightText = "CHG";
        } else if (battery.powerState == PowerState::Discharge) {
            topLeftText = formatDurationCompact(battery.estimatedTimeHours);
            topRightText = "OUT";
        } else {
            topRightText = "IDLE";
        }
    }

    if (topLeftText.length() > 0) {
        smallDisplay.setCursor(0, 0);
        smallDisplay.print(topLeftText);
    }

    if (topRightText.length() > 0) {
        int16_t textX = 0;
        int16_t textY = 0;
        uint16_t textW = 0;
        uint16_t textH = 0;
        smallDisplay.getTextBounds(topRightText, 0, 0, &textX, &textY, &textW, &textH);
        smallDisplay.setCursor(127 - static_cast<int16_t>(textW) + 1, 0);
        smallDisplay.print(topRightText);
    }

    drawSmallBatteryBar(battery.socPercent, battery.powerState, animationFrame);

    smallDisplay.setTextSize(2);
    smallDisplay.setCursor(0, 18);
    smallDisplay.print(String(battery.socPercent, 0));
    smallDisplay.print('%');

    const bool showBluetoothIcon = bluetoothEnabled
        && (bluetoothConnected || ((animationFrame / 2) % 2) == 0);

    String bottomRightText;

    if (battery.outputDisabledByProtection) {
        if ((animationFrame % 2) == 0) {
            bottomRightText = "PROT";
        }
    } else if (systemOff) {
        if ((animationFrame % 2) == 0) {
            bottomRightText = "OFF";
        }
    } else {
        float displayPowerW = updateSmallDisplayPower(
            battery.powerW,
            battery.powerState,
            millis()
        );
        bottomRightText = formatSmallPowerW(displayPowerW);
    }

    if (bottomRightText.length() > 0) {
        const int16_t rightEdge = bluetoothEnabled ? 114 : 127;
        int16_t textX = 0;
        int16_t textY = 0;
        uint16_t textW = 0;
        uint16_t textH = 0;
        smallDisplay.getTextBounds(bottomRightText, 0, 0, &textX, &textY, &textW, &textH);
        smallDisplay.setCursor(rightEdge - static_cast<int16_t>(textW) + 1, 18);
        smallDisplay.print(bottomRightText);
    }

    if (showBluetoothIcon) {
        drawSmallBluetoothIcon(119, 17);
    }

    smallDisplay.display();
}

void DisplayManager::renderMain(
    MainPage page,
    const BatteryState &battery,
    const BatteryConfig &batteryConfig,
    const UiConfig &uiConfig,
    const CoolingState &cooling,
    bool bluetoothEnabled,
    bool bluetoothConnected
) {
    if (!mainDisplayOn) {
        return;
    }

    mainDisplay.clearDisplay();
    mainDisplay.setTextColor(SH110X_WHITE);
    mainDisplay.setTextSize(1);
    mainDisplay.setCursor(0, 0);

    switch (page) {
        case MainPage::BatPower:
            renderBatPowerPage(battery, cooling);
            break;

        case MainPage::CapacityLearn:
            renderCapacityLearnPage(battery, batteryConfig);
            break;

        case MainPage::Settings:
            renderSettingsPage(batteryConfig, uiConfig);
            break;

        case MainPage::Bluetooth:
            renderBluetoothPage(bluetoothEnabled, bluetoothConnected);
            break;
    }

    mainDisplay.display();
}

float DisplayManager::updateSmallDisplayPower(
    float powerW,
    PowerState powerState,
    uint32_t nowMs
) {
    if (powerState == PowerState::Idle || fabsf(powerW) < 1.0f) {
        smallPowerFilterReady = true;
        smallPowerFilterState = powerState;
        smallPowerFilteredW = 0.0f;
        lastSmallPowerFilterMs = nowMs;
        return 0.0f;
    }

    if (!smallPowerFilterReady || smallPowerFilterState != powerState) {
        smallPowerFilterReady = true;
        smallPowerFilterState = powerState;
        smallPowerFilteredW = powerW;
        lastSmallPowerFilterMs = nowMs;
        return smallPowerFilteredW;
    }

    float dtSeconds = lastSmallPowerFilterMs == 0
        ? static_cast<float>(Config::DISPLAY_REFRESH_MS) / 1000.0f
        : static_cast<float>(nowMs - lastSmallPowerFilterMs) / 1000.0f;
    dtSeconds = clampFloat(dtSeconds, 0.001f, 2.0f);
    lastSmallPowerFilterMs = nowMs;

    float deltaW = fabsf(powerW - smallPowerFilteredW);
    float tauSeconds = deltaW >= Config::SMALL_POWER_FAST_DELTA_W
        ? Config::SMALL_POWER_FAST_TAU_SECONDS
        : Config::SMALL_POWER_SMOOTH_TAU_SECONDS;

    float alpha = 1.0f - expf(-dtSeconds / tauSeconds);
    smallPowerFilteredW += alpha * (powerW - smallPowerFilteredW);

    return smallPowerFilteredW;
}

void DisplayManager::drawSmallBatteryBar(float socPercent, PowerState powerState, uint8_t animationFrame) {
    const int x = 0;
    const int y = 9;
    const int w = 126;
    const int h = 5;

    socPercent = clampFloat(socPercent, 0.0f, 100.0f);

    smallDisplay.drawRect(x, y, w, h, 1);

    const int innerW = w - 2;
    const int fillW = static_cast<int>(roundf(innerW * socPercent / 100.0f));
    const int fillX = x + 1;

    if (fillW > 0) {
        smallDisplay.fillRect(fillX, y + 1, fillW, h - 2, 1);
    }

    if (powerState != PowerState::Charge || fillW <= 0) {
        return;
    }

    constexpr int BAND_WIDTH = 10;
    constexpr int STEP_PIXELS = 3;

    const int travelLength = fillW + BAND_WIDTH;
    const int bandX = fillX - BAND_WIDTH
        + (static_cast<int>(animationFrame) * STEP_PIXELS) % travelLength;

    const int visibleStart = max(fillX, bandX);
    const int visibleEnd = min(fillX + fillW, bandX + BAND_WIDTH);

    if (visibleEnd > visibleStart) {
        smallDisplay.fillRect(
            visibleStart,
            y + 1,
            visibleEnd - visibleStart,
            h - 2,
            0
        );
    }
}

void DisplayManager::drawSmallBluetoothIcon(int x, int y) {
    smallDisplay.drawLine(x + 3, y, x + 3, y + 14, 1);
    smallDisplay.drawLine(x + 3, y, x + 7, y + 4, 1);
    smallDisplay.drawLine(x + 7, y + 4, x + 1, y + 10, 1);
    smallDisplay.drawLine(x + 1, y + 4, x + 7, y + 10, 1);
    smallDisplay.drawLine(x + 7, y + 10, x + 3, y + 14, 1);
}

void DisplayManager::renderBatPowerPage(
    const BatteryState &battery,
    const CoolingState &cooling
) {
    mainDisplay.print("Volt:    ");
    mainDisplay.print(String(battery.voltageV, 2));
    mainDisplay.println("V");

    mainDisplay.print("Current: ");
    mainDisplay.print(String(fabsf(battery.currentA), 2));
    mainDisplay.println("A");

    mainDisplay.print("Power:   ");
    mainDisplay.print(String(fabsf(battery.powerW), 1));
    mainDisplay.println("W");

    mainDisplay.print("T power: ");
    if (cooling.powerSensorValid) {
        mainDisplay.print(String(cooling.powerTemperatureC, 1));
        mainDisplay.println("C");
    } else {
        mainDisplay.println("ERR");
    }

    mainDisplay.print("T air:   ");
    if (cooling.airSensorValid) {
        mainDisplay.print(String(cooling.airTemperatureC, 1));
        mainDisplay.println("C");
    } else {
        mainDisplay.println("ERR");
    }

    mainDisplay.print("Fan:     ");
    mainDisplay.print(cooling.fanPercent);
    mainDisplay.print('%');

    if (cooling.fault) {
        mainDisplay.print(" FAIL");
    }
}

void DisplayManager::renderCapacityLearnPage(const BatteryState &battery, const BatteryConfig &batteryConfig) {
    mainDisplay.println("CAPACITY LEARN");

    mainDisplay.print("Nominal: ");
    mainDisplay.print(String(batteryConfig.nominalCapacityWh, 0));
    mainDisplay.println("Wh");

    mainDisplay.print("Learned: ");
    mainDisplay.print(String(battery.learnedCapacityWh, 0));
    mainDisplay.println("Wh");

    mainDisplay.print("Cycle:   ");
    mainDisplay.println(learnCycleToText(battery.learnCycle));

    mainDisplay.print("Status:  ");
    mainDisplay.println(learnStatusToText(battery.learnStatus));
}

void DisplayManager::renderSettingsPage(const BatteryConfig &batteryConfig, const UiConfig &uiConfig) {
    mainDisplay.println("SETTINGS");

    mainDisplay.print("Small off: ");
    mainDisplay.print(uiConfig.smallScreenTimeoutSec);
    mainDisplay.println("s");

    mainDisplay.print("Main off:  ");
    mainDisplay.print(uiConfig.mainScreenTimeoutSec);
    mainDisplay.println("s");

    mainDisplay.print("Low cut:   ");
    mainDisplay.print(String(batteryConfig.lowCutVoltageV, 1));
    mainDisplay.println("V");

    mainDisplay.print("Charge eff:");
    mainDisplay.print(String(batteryConfig.chargeEfficiency * 100.0f, 0));
    mainDisplay.println("% auto");

    mainDisplay.print("Power lim: ");
    mainDisplay.print(String(batteryConfig.powerLimitW, 0));
    mainDisplay.println("W");
}

void DisplayManager::renderBluetoothPage(bool bluetoothEnabled, bool bluetoothConnected) {
    mainDisplay.println("BLUETOOTH");

    mainDisplay.print("Status: ");
    mainDisplay.println(bluetoothEnabled ? "ON" : "OFF");

    if (!bluetoothEnabled) {
        return;
    }

    mainDisplay.print("Device: ");
    mainDisplay.println(Config::BLE_NAME);

    mainDisplay.print("Client: ");
    mainDisplay.println(bluetoothConnected ? "connected" : "waiting");
}

void DisplayManager::drawMainProgressBar(int x, int y, int w, int h, float percent) {
    percent = clampFloat(percent, 0.0f, 100.0f);

    mainDisplay.drawRect(x, y, w, h, SH110X_WHITE);

    int fillWidth = static_cast<int>(roundf((w - 2) * percent / 100.0f));

    if (fillWidth > 0) {
        mainDisplay.fillRect(x + 1, y + 1, fillWidth, h - 2, SH110X_WHITE);
    }
}
