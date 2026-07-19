#include "BlePowerService.h"
#include "AppController.h"
#include "Config.h"
#include <cstring>

void BlePowerService::begin(AppController *appController) {
    this->appController = appController;
}

void BlePowerService::enable() {
    if (enabled) {
        return;
    }

    if (!stackInitialized) {
        BLEDevice::init(Config::BLE_NAME);
        BLEDevice::setMTU(Config::BLE_MTU);

        server = BLEDevice::createServer();
        server->setCallbacks(new ServerCallbacks(this));

        BLEService *service = server->createService("6f2a0001-5a3d-4e2c-9a73-1b21d9b00001");

        statusCharacteristic = service->createCharacteristic(
            "6f2a0002-5a3d-4e2c-9a73-1b21d9b00001",
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        statusCharacteristic->addDescriptor(new BLE2902());

        commandCharacteristic = service->createCharacteristic(
            "6f2a0003-5a3d-4e2c-9a73-1b21d9b00001",
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
        );
        commandCharacteristic->setCallbacks(new CommandCallbacks(this));

        settingsCharacteristic = service->createCharacteristic(
            "6f2a0004-5a3d-4e2c-9a73-1b21d9b00001",
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        settingsCharacteristic->addDescriptor(new BLE2902());

        service->start();

        BLEAdvertising *advertising = BLEDevice::getAdvertising();
        advertising->addServiceUUID("6f2a0001-5a3d-4e2c-9a73-1b21d9b00001");
        advertising->setScanResponse(true);

        stackInitialized = true;
    }

    enabled = true;
    connected = false;

    if (appController != nullptr) {
        statusCharacteristic->setValue(appController->makeStatusJson().c_str());
        settingsCharacteristic->setValue(appController->makeSettingsJson().c_str());
    }

    BLEDevice::getAdvertising()->start();

    Serial.println("BLE enabled");
}

void BlePowerService::disable() {
    if (!enabled) {
        return;
    }

    // BLEDevice::deinit(true) может надолго блокировать loop на ESP32-S3.
    // Оставляем стек и GATT-сервис в памяти, но прекращаем рекламу и обмен.
    BLEDevice::getAdvertising()->stop();
    enabled = false;
    connected = false;
    lastNotifyMs = 0;

    Serial.println("BLE disabled (stack kept initialized)");
}

void BlePowerService::toggle() {
    if (enabled) {
        disable();
    } else {
        enable();
    }
}

void BlePowerService::update() {
    String command;
    if (takePendingCommand(command) && appController != nullptr) {
        appController->handleBleCommand(command);
    }

    if (!enabled || !connected || statusCharacteristic == nullptr) {
        return;
    }

    uint32_t now = millis();

    if (now - lastNotifyMs < Config::BLE_NOTIFY_MS) {
        return;
    }

    lastNotifyMs = now;

    if (appController != nullptr) {
        notifyStatus(appController->makeStatusJson());
    }
}

bool BlePowerService::isEnabled() const {
    return enabled;
}

bool BlePowerService::isConnected() const {
    return connected;
}

void BlePowerService::notifyStatus(const String &statusJson) {
    if (!enabled || statusCharacteristic == nullptr) {
        return;
    }

    if (statusJson.length() > Config::BLE_MAX_VALUE_BYTES) {
        Serial.print("BLE response rejected, bytes: ");
        Serial.println(statusJson.length());

        const char *errorJson = "{\"type\":\"error\",\"error\":\"response_too_large\"}";
        statusCharacteristic->setValue(errorJson);
    } else {
        statusCharacteristic->setValue(statusJson.c_str());
    }

    if (connected) {
        statusCharacteristic->notify();
    }
}


void BlePowerService::notifySettings(const String &settingsJson) {
    if (!enabled || settingsCharacteristic == nullptr) {
        return;
    }

    if (settingsJson.length() > Config::BLE_MAX_VALUE_BYTES) {
        Serial.print("BLE settings rejected, bytes: ");
        Serial.println(settingsJson.length());

        const char *errorJson = "{\"type\":\"error\",\"error\":\"response_too_large\"}";
        settingsCharacteristic->setValue(errorJson);
    } else {
        settingsCharacteristic->setValue(settingsJson.c_str());
    }

    if (connected) {
        settingsCharacteristic->notify();
    }
}

void BlePowerService::queueCommand(const std::string &command) {
    portENTER_CRITICAL(&commandMux);

    size_t length = command.length();
    if (length >= COMMAND_BUFFER_SIZE) {
        length = COMMAND_BUFFER_SIZE - 1;
    }

    memcpy(pendingCommand, command.data(), length);
    pendingCommand[length] = '\0';
    commandPending = true;

    portEXIT_CRITICAL(&commandMux);
}

bool BlePowerService::takePendingCommand(String &command) {
    char localBuffer[COMMAND_BUFFER_SIZE];

    portENTER_CRITICAL(&commandMux);

    if (!commandPending) {
        portEXIT_CRITICAL(&commandMux);
        return false;
    }

    memcpy(localBuffer, pendingCommand, COMMAND_BUFFER_SIZE);
    pendingCommand[0] = '\0';
    commandPending = false;

    portEXIT_CRITICAL(&commandMux);

    localBuffer[COMMAND_BUFFER_SIZE - 1] = '\0';
    command = String(localBuffer);
    return true;
}

void BlePowerService::ServerCallbacks::onConnect(BLEServer *server) {
    service->connected = true;
}

void BlePowerService::ServerCallbacks::onDisconnect(BLEServer *server) {
    service->connected = false;

    if (service->enabled) {
        server->getAdvertising()->start();
    }
}

void BlePowerService::CommandCallbacks::onWrite(BLECharacteristic *characteristic) {
    // Callback BLE выполняется в системной задаче Bluetooth.
    // Здесь нельзя формировать JSON, сохранять NVS или вызывать notify().
    service->queueCommand(characteristic->getValue());
}
