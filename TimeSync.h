#ifndef TIMESYNC_H
#define TIMESYNC_H

#include <Arduino.h>
#include "RtcManager.h"

// Synchronisation NTP NON BLOQUANTE (n'arrête ni l'écran ni le serveur web).
namespace TimeSync {
    void request();                 // lance (ou relance) une synchro NTP
    bool pending();                 // true tant qu'on attend la réponse
    bool poll(RtcManager& rtc);     // à appeler souvent : true UNE fois, quand l'heure vient d'être réglée
}

#endif
