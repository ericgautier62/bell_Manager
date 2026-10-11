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

void RelayController::ringNow(uint8_t pulseCount, uint16_t pulseOnMs, uint16_t pulseOffMs) {
    if (_held) return;
    for (uint8_t i = 0; i < pulseCount; i++) {
        setRelay(true);
        delay(pulseOnMs);
        setRelay(false);
        if (i < pulseCount - 1) delay(pulseOffMs);
    }
}

void RelayController::holdOn() {
    if (!_held) {
        _held = true;
        _heldSince = millis();
        setRelay(true);
    }
    _lastFeed = millis();
}

void RelayController::holdOff() {
    _held = false;
    setRelay(false);
}

void RelayController::feedHold() {
    _lastFeed = millis();
}

void RelayController::update() {
    if (!_held) return;
    unsigned long now = millis();
    if (now - _lastFeed > BELL_HOLD_WATCHDOG_MS || now - _heldSince > BELL_HOLD_MAX_MS) {
        holdOff();
    }
}
