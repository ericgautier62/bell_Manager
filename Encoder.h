#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

class Encoder {
public:
    Encoder(uint8_t clk, uint8_t dt, uint8_t sw);
    void begin();

    // N'gère plus que le bouton (lecture non bloquante) : la rotation
    // est désormais traitée en interruption, indépendamment du polling.
    void update();

    int getCounter();          // lecture thread-safe
    void resetCounter();
    bool isButtonPressed();

    // Etat instantané (déjà débouncé) du bouton : true tant qu'il est enfoncé.
    // Nécessite que update() soit appelée régulièrement.
    bool isButtonHeld() const;

    // Oublie un appui en attente (ex: l'appui qui vient d'ouvrir un écran).
    void clearButtonEvent();

    // Lecture non destructive : indique s'il y a une rotation ou un appui
    // en attente, sans les consommer. Utile pour interrompre une animation
    // de veille dès qu'une interaction survient.
    bool hasPendingActivity();

    // Appelée uniquement par la routine d'interruption statique.
    void IRAM_ATTR handleInterrupt();

private:
    uint8_t _clk, _dt, _sw;

    // --- Etat partagé avec l'ISR ---
    volatile int _counter;
    volatile uint8_t _lastState;   // 2 bits: (CLK<<1)|DT précédents
    portMUX_TYPE _mux;

    // --- Debounce bouton non bloquant (remplace le delay() bloquant) ---
    bool _lastBtnReading;
    bool _lastBtnStable;
    unsigned long _lastBtnChangeMs;
    bool _btnPressed;

    static Encoder* _instance;     // une seule instance supportée (cas d'usage du projet)
    static void IRAM_ATTR isrTrampoline();
};

#endif
