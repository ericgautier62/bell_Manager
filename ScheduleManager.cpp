#include "ScheduleManager.h"
#include <Preferences.h>
#include "config.h"

void ScheduleManager::begin() {
    Preferences prefs;
    prefs.begin("bell_sched", false);
    size_t bytes = prefs.getBytes("entries", _entries, sizeof(_entries));
    _count = bytes / sizeof(ScheduleEntry);
    bool firstBoot = !prefs.getBool("init", false);   // vrai seulement au tout premier démarrage
    prefs.end();

    if (firstBoot) {
        loadDefaults();
        save();
    }
}

void ScheduleManager::save() {
    Preferences prefs;
    prefs.begin("bell_sched", false);
    prefs.putBool("init", true);
    if (_count == 0) prefs.remove("entries");   // putBytes(len=0) ne ferait rien
    else prefs.putBytes("entries", _entries, _count * sizeof(ScheduleEntry));
    prefs.end();
}

void ScheduleManager::loadDefaults() {
    // Exemple lundi à vendredi (bits 1..5 ; 0=dimanche ... 6=samedi). A adapter via l'interface web.
    const uint8_t JOURS_OUVRES = 0b0111110;
    _count = 0;
    _entries[_count++] = {7, 30, JOURS_OUVRES, true};
    _entries[_count++] = {12, 0, JOURS_OUVRES, true};
    _entries[_count++] = {17, 0, JOURS_OUVRES, true};
}

void ScheduleManager::replaceAll(const ScheduleEntry* list, uint8_t n) {
    if (n > MAX_SCHEDULES) n = MAX_SCHEDULES;
    for (uint8_t i = 0; i < n; i++) _entries[i] = list[i];
    _count = n;
    save();
}

void ScheduleManager::update(RtcManager& rtc, ExceptionCalendar& holidays, ExtraDays& extras, RelayController& relay) {
    DateTime now = rtc.now();

    // Minute déjà traitée -> rien à faire
    if (now.year() == _lastYear && now.month() == _lastMonth && now.day() == _lastDay &&
        now.hour() == _lastHour && now.minute() == _lastMinute) {
        return;
    }
    _lastYear = now.year();
    _lastMonth = now.month();
    _lastDay = now.day();
    _lastHour = now.hour();
    _lastMinute = now.minute();

    if (relay.isHeld()) return;   // sonnerie manuelle en cours : on ne s'y superpose pas

    uint8_t effectiveDow = now.dayOfTheWeek();   // 0=dimanche ... 6=samedi
    uint8_t tpl;
    if (extras.find(now.year(), now.month(), now.day(), &tpl)) {
        effectiveDow = tpl;                      // jour exceptionnel : horaire d'un autre jour
    } else if (holidays.isException(now.year(), now.month(), now.day())) {
        return;                                  // férié / vacances
    }

    for (uint8_t i = 0; i < _count; i++) {
        const ScheduleEntry& e = _entries[i];
        if (!e.enabled) continue;
        if (e.hour == now.hour() && e.minute == now.minute() && (e.daysMask & (1 << effectiveDow))) {
            relay.ringNow(BELL_PULSE_COUNT, BELL_PULSE_ON_MS, BELL_PULSE_OFF_MS);
            break;
        }
    }
}
