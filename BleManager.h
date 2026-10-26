#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "WebDashboard.h"

// BLE GATT transport. The app sends a REQ header, sequential binary chunks,
// then COMMIT; responses use RESULT plus the same offset-framed chunks.
class BleManager {
public:
    explicit BleManager(WebDashboard& api);
    void begin();
    void service();
    void openPairingWindow(uint32_t durationMs = 300000UL);

private:
    static const size_t PACKET_CAPACITY = 247;
    struct Packet { uint8_t kind; uint16_t length; uint8_t bytes[PACKET_CAPACITY]; };
    class ServerEvents;
    class CharacteristicEvents;

    WebDashboard& _api;
    BLEServer* _server = nullptr;
    BLECharacteristic* _control = nullptr;
    BLECharacteristic* _input = nullptr;
    BLECharacteristic* _output = nullptr;
    QueueHandle_t _queue = nullptr;
    bool _initialized = false;
    bool _windowOpen = false;
    uint32_t _windowClosesAt = 0;
    uint16_t _requestId = 0;
    String _requestMethod;
    String _requestPath;
    size_t _expectedBody = 0;
    String _requestBody;
    String _responseBody;
    size_t _responseOffset = 0;

    bool windowOpen() const;
    void enqueue(uint8_t kind, const std::string& value);
    void processControl(const String& command);
    void sendText(const String& value);
    void sendResponse(const String& value);
    void sendNextResponseChunk();
};

#endif
