#include "BleManager.h"
#include <BLEDevice.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <esp_system.h>
#include "config.h"

namespace {
const char* SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const char* CONTROL_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
const char* INPUT_UUID = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";
const char* OUTPUT_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
const uint8_t FRAME_REQUEST = 0xD1;
const uint8_t FRAME_RESPONSE = 0xD2;
const size_t MAX_BODY = 20000;
}

class BleManager::ServerEvents : public BLEServerCallbacks {
public:
    explicit ServerEvents(BleManager* owner) : _owner(owner) {}
    void onDisconnect(BLEServer*) override {
        if (_owner->windowOpen()) _owner->_server->startAdvertising();
    }
private:
    BleManager* _owner;
};

class BleManager::CharacteristicEvents : public BLECharacteristicCallbacks {
public:
    CharacteristicEvents(BleManager* owner, uint8_t kind) : _owner(owner), _kind(kind) {}
    void onWrite(BLECharacteristic* characteristic) override {
        _owner->enqueue(_kind, characteristic->getValue());
    }
private:
    BleManager* _owner;
    uint8_t _kind;
};

BleManager::BleManager(WebDashboard& api) : _api(api) {}

void BleManager::begin() {
    if (_initialized) return;
    _queue = xQueueCreate(16, sizeof(Packet));
    if (!_queue) {
        Serial.println("[BLE] echec creation file de reception");
        return;
    }

    BLEDevice::init("KairosLab");
    BLEDevice::setMTU(247);
    _server = BLEDevice::createServer();
    _server->setCallbacks(new ServerEvents(this));
    BLEService* service = _server->createService(SERVICE_UUID);

    _control = service->createCharacteristic(CONTROL_UUID, BLECharacteristic::PROPERTY_WRITE);
    _control->setCallbacks(new CharacteristicEvents(this, 1));
    _input = service->createCharacteristic(INPUT_UUID, BLECharacteristic::PROPERTY_WRITE);
    _input->setCallbacks(new CharacteristicEvents(this, 2));
    _output = service->createCharacteristic(OUTPUT_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    _output->addDescriptor(new BLE2902());
    service->start();
    BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
    BLEDevice::getAdvertising()->setScanResponse(true);
    _initialized = true;
}

bool BleManager::windowOpen() const {
    return _windowOpen && (int32_t)(millis() - _windowClosesAt) < 0;
}

void BleManager::openPairingWindow(uint32_t durationMs) {
    begin();
    if (!_initialized) return;
    _windowOpen = true;
    _windowClosesAt = millis() + durationMs;
    BLEDevice::getAdvertising()->start();
    Serial.printf("[BLE] fenetre ouverte pour %lu s\n", (unsigned long)(durationMs / 1000));
}

void BleManager::enqueue(uint8_t kind, const std::string& value) {
    if (!windowOpen() || value.size() > PACKET_CAPACITY || (kind == 1 && value.size() >= PACKET_CAPACITY)) return;
    Packet packet{};
    packet.kind = kind;
    packet.length = value.size();
    memcpy(packet.bytes, value.data(), value.size());
    if (kind == 1) packet.bytes[packet.length] = 0;
    xQueueSend(_queue, &packet, 0);
}

void BleManager::sendText(const String& value) {
    if (!_output || _server->getConnectedCount() == 0) return;
    _output->setValue(std::string(value.c_str()));
    _output->notify();
}

void BleManager::sendResponse(const String& value) {
    _responseBody = value;
    _responseOffset = 0;
    sendText("RESULT|" + String(_requestId) + "|" + String(_responseBody.length()));
}

void BleManager::processControl(const String& command) {
    if (command.startsWith("REQ|")) {
        unsigned int id = 0, bodyLength = 0;
        char method[8] = {}, path[32] = {};
        if (sscanf(command.c_str(), "REQ|%u|%7[^|]|%31[^|]|%u", &id, method, path, &bodyLength) != 4 || bodyLength > MAX_BODY) {
            sendText("ERROR|request_format"); return;
        }
        _requestId = (uint16_t)id;
        _requestMethod = method;
        _requestPath = path;
        _expectedBody = bodyLength;
        _requestBody = "";
        if (_expectedBody == 0) {
            sendResponse(_api.processApiRequest(method, path, ""));
        } else {
            _requestBody.reserve(_expectedBody);
            sendText("READY|" + String(_requestId));
        }
        return;
    }

    if (command.startsWith("COMMIT|")) {
        unsigned int id = 0;
        if (sscanf(command.c_str(), "COMMIT|%u", &id) != 1 || id != _requestId ||
            _expectedBody == 0 || _requestBody.length() != _expectedBody) {
            sendText("ERROR|body_incomplete"); return;
        }
        String response = _api.processApiRequest(_requestMethod, _requestPath, _requestBody);
        _expectedBody = 0;
        _requestBody = "";
        sendResponse(response);
        return;
    }
    sendText("ERROR|unknown_command");
}

void BleManager::sendNextResponseChunk() {
    if (!_responseBody.length() || !_output || _server->getConnectedCount() == 0) return;
    const size_t mtu = _server->getPeerMTU(_server->getConnId());
    const size_t payloadLimit = (mtu > 6 && mtu - 6 < PACKET_CAPACITY - 3) ? mtu - 6 : 16;
    size_t remaining = _responseBody.length() - _responseOffset;
    size_t count = remaining < payloadLimit ? remaining : payloadLimit;
    uint8_t frame[PACKET_CAPACITY];
    frame[0] = FRAME_RESPONSE;
    frame[1] = _responseOffset & 0xff;
    frame[2] = (_responseOffset >> 8) & 0xff;
    memcpy(frame + 3, _responseBody.c_str() + _responseOffset, count);
    _output->setValue(frame, count + 3);
    _output->notify();
    _responseOffset += count;
    if (_responseOffset >= _responseBody.length()) _responseBody = "";
}

void BleManager::service() {
    if (!_initialized) return;
    if (_windowOpen && !windowOpen()) {
        _windowOpen = false;
        BLEDevice::getAdvertising()->stop();
        if (_server->getConnectedCount()) _server->disconnect(_server->getConnId());
        Serial.println("[BLE] fenetre fermee");
    }

    Packet packet;
    for (uint8_t drained = 0; drained < 16 && xQueueReceive(_queue, &packet, 0) == pdTRUE; ++drained) {
        if (!windowOpen()) break;
        if (packet.kind == 1) {
            processControl(String((const char*)packet.bytes));
        } else if (packet.kind == 2 && packet.length >= 3 && packet.bytes[0] == FRAME_REQUEST) {
            size_t offset = packet.bytes[1] | ((size_t)packet.bytes[2] << 8);
            size_t count = packet.length - 3;
            if (offset == _requestBody.length() && offset + count <= _expectedBody) {
                _requestBody.concat((const char*)packet.bytes + 3, count);
            } else {
                _expectedBody = 0;
                _requestBody = "";
                sendText("ERROR|chunk_order");
            }
        }
    }
    sendNextResponseChunk();
}
