#include "RelayController.h"
#include "config.h"

RelayController::RelayController(uint8_t pin, bool activeLow)
    : _pin(pin), _activeLow(activeLow) {}

void RelayController::begin() {
    // On fixe d'abord le niveau "repos" puis on passe en sortie : évite
    // une impulsion parasite au démarrage avec les modules actifs-bas.
    digitalWrite(_pin, _activeLow ? HIGH : LOW);
    pinMode(_pin, OUTPUT);
    setRelay(false);
}

void RelayController::setRelay(bool on) {
    bool level = _activeLow ? !on : on;
    digitalWrite(_pin, level ? HIGH : LOW);
}

void RelayController::ringFor(uint32_t durationMs) {
    if (_held || durationMs == 0) return;
    _timedRinging = true;
    _ringStarted = millis();
    _ringDurationMs = durationMs;
    setRelay(true);
}

void RelayController::holdOn() {
    if (!_held) {
        _timedRinging = false;
        _held = true;
        _heldSince = millis();
        setRelay(true);
    }
    _lastFeed = millis();
}

void RelayController::holdOff() {
    if (!_held) return;
    _held = false;
    setRelay(false);
}

void RelayController::feedHold() {
    _lastFeed = millis();
}

void RelayController::update() {
    unsigned long now = millis();
    if (_timedRinging && now - _ringStarted >= _ringDurationMs) {
        _timedRinging = false;
        setRelay(false);
    }
    if (_held && (now - _lastFeed > BELL_HOLD_WATCHDOG_MS || now - _heldSince > BELL_HOLD_MAX_MS)) {
        holdOff();
    }
}
