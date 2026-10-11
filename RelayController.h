#ifndef RELAYCONTROLLER_H
#define RELAYCONTROLLER_H

#include <Arduino.h>

class RelayController {
public:
    RelayController(uint8_t pin, bool activeLow = true);

    // A appeler dans setup() : relais au repos immédiatement.
    void begin();

    // Sonnerie programmée : activation continue et non bloquante.
    // Ignorée si une sonnerie "maintenue" est active.
    void ringFor(uint32_t durationMs);

    // Sonnerie "maintenue" (bouton OLED ou bouton web) :
    // le relais reste fermé tant que feedHold() est appelée régulièrement.
    void holdOn();
    void holdOff();
    void feedHold();              // "heartbeat" : prouve que le bouton est toujours enfoncé
    bool isHeld() const { return _held; }

    // A appeler en continu : coupe le relais si le heartbeat s'arrête
    // (page web fermée, Wi-Fi perdu) ou si la durée maximale est dépassée.
    void update();

private:
    uint8_t _pin;
    bool _activeLow;
    bool _held = false;
    bool _timedRinging = false;
    unsigned long _ringStarted = 0;
    uint32_t _ringDurationMs = 0;
    unsigned long _heldSince = 0;
    unsigned long _lastFeed = 0;
    void setRelay(bool on);
};

#endif
