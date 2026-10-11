#include "ExceptionCalendar.h"
#include <Preferences.h>

void ExceptionCalendar::begin() {
    Preferences prefs;
    prefs.begin("bell_exc", false);
    size_t bytes = prefs.getBytes("dates", _dates, sizeof(_dates));
    _count = bytes / sizeof(ExceptionDate);
    prefs.end();
}

void ExceptionCalendar::save() {
    Preferences prefs;
    prefs.begin("bell_exc", false);
    if (_count == 0) prefs.remove("dates");   // putBytes(len=0) ne ferait rien
    else prefs.putBytes("dates", _dates, _count * sizeof(ExceptionDate));
    prefs.end();
}

bool ExceptionCalendar::isException(uint16_t year, uint8_t month, uint8_t day) const {
    for (uint16_t i = 0; i < _count; i++) {
        if (_dates[i].year == year && _dates[i].month == month && _dates[i].day == day) return true;
    }
    return false;
}

bool ExceptionCalendar::addException(uint16_t year, uint8_t month, uint8_t day) {
    if (_count >= MAX_EXCEPTIONS) return false;
    _dates[_count++] = {year, month, day};
    save();
    return true;
}

void ExceptionCalendar::replaceAll(const ExceptionDate* list, uint16_t n) {
    if (n > MAX_EXCEPTIONS) n = MAX_EXCEPTIONS;
    for (uint16_t i = 0; i < n; i++) _dates[i] = list[i];
    _count = n;
    save();
}
