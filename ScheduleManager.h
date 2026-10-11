#ifndef SCHEDULEMANAGER_H
#define SCHEDULEMANAGER_H

#include <Arduino.h>
#include "Schedule.h"
#include "RtcManager.h"
#include "ExceptionCalendar.h"
#include "ExtraDays.h"
#include "RelayController.h"

class ScheduleManager {
public:
    static const uint8_t MAX_SCHEDULES = 20;

    // Charge les horaires (NVS). Premier démarrage : jeu par défaut lun-ven.
    void begin();
    void save();

    void replaceAll(const ScheduleEntry* list, uint8_t n);   // remplace tout + sauvegarde
    uint8_t getCount() const { return _count; }
    const ScheduleEntry& getEntry(uint8_t idx) const { return _entries[idx]; }

    // A appeler en continu. Règles, par ordre de priorité :
    //  1. jour EXCEPTIONNEL -> on applique l'horaire du jour modèle choisi
    //  2. jour férié / vacances -> aucune sonnerie
    //  3. sinon -> horaire du jour de la semaine
    // Un seul déclenchement par minute.
    void update(RtcManager& rtc, ExceptionCalendar& holidays, ExtraDays& extras, RelayController& relay);

private:
    ScheduleEntry _entries[MAX_SCHEDULES];
    uint8_t _count = 0;

    uint16_t _lastYear = 0;
    uint8_t _lastMonth = 0, _lastDay = 0, _lastHour = 0, _lastMinute = 255;

    void loadDefaults();
};

#endif
