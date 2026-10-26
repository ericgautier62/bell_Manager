#include "ExtraDays.h"
#include <Preferences.h>

namespace {
struct LegacyExtraDay { uint16_t year; uint8_t month, day, tplDow; };

int32_t dayNumber(uint16_t y, uint8_t m, uint8_t d) {
    y -= m <= 2;
    const int32_t era = y / 400;
    const uint32_t yoe = y - era * 400;
    const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + (int32_t)doe;
}
}

void ExtraDays::begin() {
    Preferences prefs;
    prefs.begin("bell_extra", false);
    uint8_t version = prefs.getUChar("ver", 0);
    if (version >= 2) {
        size_t bytes = prefs.getBytes("days", _days, sizeof(_days));
        _count = bytes / sizeof(ExtraDay);
        if (_count > MAX_EXTRAS) _count = MAX_EXTRAS;
    } else {
        LegacyExtraDay old[MAX_EXTRAS];
        size_t bytes = prefs.getBytes("days", old, sizeof(old));
        _count = bytes / sizeof(LegacyExtraDay);
        if (_count > MAX_EXTRAS) _count = MAX_EXTRAS;
        for (uint16_t i = 0; i < _count; ++i) {
            ExtraDay& e = _days[i];
            memset(&e, 0, sizeof(e));
            e.year = old[i].year; e.month = old[i].month; e.day = old[i].day;
            e.endYear = old[i].year; e.endMonth = old[i].month; e.endDay = old[i].day;
            e.tplDow = old[i].tplDow; e.repeatDays = 0;
        }
        prefs.putUChar("ver", 2);
        if (_count) prefs.putBytes("days", _days, _count * sizeof(ExtraDay));
    }
    prefs.end();
}

void ExtraDays::save() {
    Preferences prefs;
    prefs.begin("bell_extra", false);
    prefs.putUChar("ver", 2);
    if (_count == 0) prefs.remove("days");
    else prefs.putBytes("days", _days, _count * sizeof(ExtraDay));
    prefs.end();
}

const ExtraDay* ExtraDays::find(uint16_t year, uint8_t month, uint8_t day) const {
    const int32_t today = dayNumber(year, month, day);
    for (uint16_t i = 0; i < _count; i++) {
        const ExtraDay& e = _days[i];
        const int32_t start = dayNumber(e.year, e.month, e.day);
        const int32_t end = dayNumber(e.endYear, e.endMonth, e.endDay);
        if (today < start || today > end) continue;
        if (today == start || (e.repeatDays && (today - start) % e.repeatDays == 0)) return &e;
    }
    return nullptr;
}

void ExtraDays::replaceAll(const ExtraDay* list, uint16_t n) {
    if (n > MAX_EXTRAS) n = MAX_EXTRAS;
    for (uint16_t i = 0; i < n; i++) _days[i] = list[i];
    _count = n;
    save();
}
