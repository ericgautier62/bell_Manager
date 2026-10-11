#include "ExtraDays.h"
#include <Preferences.h>

void ExtraDays::begin() {
    Preferences prefs;
    prefs.begin("bell_extra", false);
    size_t bytes = prefs.getBytes("days", _days, sizeof(_days));
    _count = bytes / sizeof(ExtraDay);
    prefs.end();
}

void ExtraDays::save() {
    Preferences prefs;
    prefs.begin("bell_extra", false);
    if (_count == 0) prefs.remove("days");
    else prefs.putBytes("days", _days, _count * sizeof(ExtraDay));
    prefs.end();
}

bool ExtraDays::find(uint16_t year, uint8_t month, uint8_t day, uint8_t* tplDow) const {
    for (uint16_t i = 0; i < _count; i++) {
        if (_days[i].year == year && _days[i].month == month && _days[i].day == day) {
            if (tplDow) *tplDow = _days[i].tplDow;
            return true;
        }
    }
    return false;
}

void ExtraDays::replaceAll(const ExtraDay* list, uint16_t n) {
    if (n > MAX_EXTRAS) n = MAX_EXTRAS;
    for (uint16_t i = 0; i < n; i++) _days[i] = list[i];
    _count = n;
    save();
}
