#ifndef RTCMANAGER_H
#define RTCMANAGER_H

#include <RTClib.h>

// Source d'heure unique du projet.
//  - Si un DS3231 est présent : il fait foi (heure conservée hors tension).
//  - Sinon : repli sur l'horloge système de l'ESP32 (valide seulement après une
//    synchro NTP ou un réglage manuel, et perdue à chaque coupure de courant).
class RtcManager {
public:
    bool begin();                       // true si un DS3231 est détecté
    bool hasHardware() const { return _hw; }

    DateTime now();                     // heure locale (Cameroun, WAT)
    void adjust(const DateTime& dt);    // règle le DS3231 (si présent) + l'horloge système

    // true si l'heure est exploitable (DS3231 n'ayant pas perdu l'alimentation,
    // ou horloge système déjà réglée).
    bool hasValidTime();

private:
    RTC_DS3231 _rtc;
    bool _hw = false;
};

#endif
