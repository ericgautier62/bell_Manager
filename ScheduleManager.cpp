#include "ScheduleManager.h"
#include <Preferences.h>
#include "config.h"

void ScheduleManager::begin() {
    struct LegacyScheduleEntry { uint8_t hour, minute, daysMask; bool enabled; };
    static LegacyScheduleEntry legacy[MAX_SCHEDULES];
    Preferences prefs;
    prefs.begin("bell_sched", false);
    uint8_t version = prefs.getUChar("ver", 0);
    size_t bytes = prefs.getBytesLength("entries");
    bool migrated = false;
    if (version >= 2) {
        size_t read = prefs.getBytes("entries", _entries, sizeof(_entries));
        _count = read / sizeof(ScheduleEntry);
        if (_count > MAX_SCHEDULES) _count = MAX_SCHEDULES;
    } else if (bytes > 0) {
        size_t read = prefs.getBytes("entries", legacy, sizeof(legacy));
        _count = read / sizeof(LegacyScheduleEntry);
        if (_count > MAX_SCHEDULES) _count = MAX_SCHEDULES;
        for (uint8_t i = 0; i < _count; i++) {
            _entries[i] = { legacy[i].hour, legacy[i].minute, legacy[i].daysMask, legacy[i].enabled, 2 };
        }
        migrated = true;
    } else {
        _count = 0;
    }
    bool firstBoot = !prefs.getBool("init", false);   // vrai seulement au tout premier démarrage
    prefs.end();

    if (firstBoot) {
        loadDefaults();
        save();
    } else if (migrated || version < 2) save();
}

void ScheduleManager::save() {
    Preferences prefs;
    prefs.begin("bell_sched", false);
    prefs.putBool("init", true);
    prefs.putUChar("ver", 2);
    if (_count == 0) prefs.remove("entries");   // putBytes(len=0) ne ferait rien
    else prefs.putBytes("entries", _entries, _count * sizeof(ScheduleEntry));
    prefs.end();
}

void ScheduleManager::loadDefaults() {
    // Exemple lundi à vendredi (bits 1..5 ; 0=dimanche ... 6=samedi). A adapter via l'interface web.
    const uint8_t JOURS_OUVRES = 0b0111110;
    _count = 0;
    _entries[_count++] = {7, 30, JOURS_OUVRES, true, 2};
    _entries[_count++] = {12, 0, JOURS_OUVRES, true, 2};
    _entries[_count++] = {17, 0, JOURS_OUVRES, true, 2};
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
            relay.ringFor((uint32_t)e.durationSeconds * 1000UL);
            break;
        }
    }
}
