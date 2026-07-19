#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

class AppController;

class BlePowerService {
public:
    void begin(AppController *appController);

    void enable();
    void disable();
    void toggle();

    void update();

    bool isEnabled() const;
    bool isConnected() const;

    void notifyStatus(const String &statusJson);
    void notifySettings(const String &settingsJson);

    void queueCommand(const std::string &command);

private:
    AppController *appController = nullptr;

    bool enabled = false;
    bool connected = false;
    bool stackInitialized = false;

    BLEServer *server = nullptr;
    BLECharacteristic *statusCharacteristic = nullptr;
    BLECharacteristic *commandCharacteristic = nullptr;
    BLECharacteristic *settingsCharacteristic = nullptr;

    uint32_t lastNotifyMs = 0;

    static constexpr size_t COMMAND_BUFFER_SIZE = 128;
    char pendingCommand[COMMAND_BUFFER_SIZE] = {0};
    bool commandPending = false;
    portMUX_TYPE commandMux = portMUX_INITIALIZER_UNLOCKED;

    bool takePendingCommand(String &command);

    class ServerCallbacks : public BLEServerCallbacks {
    public:
        explicit ServerCallbacks(BlePowerService *service) {
            this->service = service;
        }

        void onConnect(BLEServer *server) override;
        void onDisconnect(BLEServer *server) override;

    private:
        BlePowerService *service = nullptr;
    };

    class CommandCallbacks : public BLECharacteristicCallbacks {
    public:
        explicit CommandCallbacks(BlePowerService *service) {
            this->service = service;
        }

        void onWrite(BLECharacteristic *characteristic) override;

    private:
        BlePowerService *service = nullptr;
    };
};
