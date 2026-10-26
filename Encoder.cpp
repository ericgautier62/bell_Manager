#include "Encoder.h"
#include "config.h"

Encoder* Encoder::_instance = nullptr;

// Table de transition quadrature (full-step). L'index est construit à partir
// de l'état précédent (2 bits) et de l'état courant (2 bits) de CLK/DT.
// Une transition invalide (rebond, glitch) donne 0 -> ignorée automatiquement.
static const int8_t QUAD_TABLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

Encoder::Encoder(uint8_t clk, uint8_t dt, uint8_t sw)
    : _clk(clk), _dt(dt), _sw(sw),
      _counter(0), _lastState(0), _quarterSteps(0),
      _mux(portMUX_INITIALIZER_UNLOCKED),
      _lastBtnReading(HIGH), _lastBtnStable(HIGH),
      _lastBtnChangeMs(0), _btnPressed(false) {}

void Encoder::begin() {
    pinMode(_clk, INPUT_PULLUP);
    pinMode(_dt, INPUT_PULLUP);
    pinMode(_sw, INPUT_PULLUP);

    _instance = this;
    _lastState = (digitalRead(_clk) << 1) | digitalRead(_dt);
    _quarterSteps = 0;

    // On écoute les deux voies : c'est nécessaire pour que la table
    // de transition quadrature fonctionne correctement.
    attachInterrupt(digitalPinToInterrupt(_clk), isrTrampoline, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_dt),  isrTrampoline, CHANGE);

    _lastBtnReading = digitalRead(_sw);
    _lastBtnStable  = _lastBtnReading;
    _lastBtnChangeMs = millis();
    _btnPressed = false;
}

void IRAM_ATTR Encoder::isrTrampoline() {
    if (_instance) _instance->handleInterrupt();
}

void IRAM_ATTR Encoder::handleInterrupt() {
    uint8_t clk = digitalRead(_clk);
    uint8_t dt  = digitalRead(_dt);
    uint8_t current = (clk << 1) | dt;
    uint8_t index = (_lastState << 2) | current;

    int8_t movement = QUAD_TABLE[index];
    if (movement != 0) {
        portENTER_CRITICAL_ISR(&_mux);
        _quarterSteps += movement;
        if (_quarterSteps >= ENCODER_TRANSITIONS_PER_STEP) {
            ++_counter;
            _quarterSteps = 0;
        } else if (_quarterSteps <= -ENCODER_TRANSITIONS_PER_STEP) {
            --_counter;
            _quarterSteps = 0;
        }
        portEXIT_CRITICAL_ISR(&_mux);
    }
    _lastState = current;
}

int Encoder::getCounter() {
    portENTER_CRITICAL(&_mux);
    int val = _counter;
    portEXIT_CRITICAL(&_mux);
    return val;
}

void Encoder::resetCounter() {
    portENTER_CRITICAL(&_mux);
    _counter = 0;
    _quarterSteps = 0;
    portEXIT_CRITICAL(&_mux);
}

// Debounce non bloquant du bouton (remplace le delay() de l'ancienne version,
// qui gelait toute la boucle principale à chaque appui).
void Encoder::update() {
    bool reading = digitalRead(_sw);

    if (reading != _lastBtnReading) {
        _lastBtnChangeMs = millis();
        _lastBtnReading = reading;
    }

    if ((millis() - _lastBtnChangeMs) > ENCODER_DEBOUNCE_MS) {
        if (reading != _lastBtnStable) {
            _lastBtnStable = reading;
            if (_lastBtnStable == LOW) {
                _btnPressed = true;
            }
        }
    }
}

bool Encoder::isButtonPressed() {
    bool ret = _btnPressed;
    _btnPressed = false;
    return ret;
}

bool Encoder::isButtonHeld() const {
    return _lastBtnStable == LOW;
}

void Encoder::clearButtonEvent() {
    _btnPressed = false;
}

bool Encoder::hasPendingActivity() {
    portENTER_CRITICAL(&_mux);
    bool rotated = (_counter != 0);
    portEXIT_CRITICAL(&_mux);
    return rotated || _btnPressed;
}
