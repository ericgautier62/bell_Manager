#include "RtcManager.h"
#include "config.h"
#include <time.h>
#include <sys/time.h>

bool RtcManager::begin() {
    setenv("TZ", TZ_POSIX, 1);   // heure locale pour localtime()/mktime()
    tzset();
    _hw = _rtc.begin();
    return _hw;
}

DateTime RtcManager::now() {
    if (_hw) return _rtc.now();
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    return DateTime(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                    tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
}

void RtcManager::adjust(const DateTime& dt) {
    if (_hw) _rtc.adjust(dt);

    // On règle aussi l'horloge système (utile au repli sans DS3231)
    struct tm tmv = {};
    tmv.tm_year = dt.year() - 1900;
    tmv.tm_mon  = dt.month() - 1;
    tmv.tm_mday = dt.day();
    tmv.tm_hour = dt.hour();
    tmv.tm_min  = dt.minute();
    tmv.tm_sec  = dt.second();
    tmv.tm_isdst = 0;
    struct timeval tv = { mktime(&tmv), 0 };
    settimeofday(&tv, nullptr);
}

bool RtcManager::hasValidTime() {
    if (_hw) return !_rtc.lostPower();
    return time(nullptr) > 1704067200;   // après le 1er janvier 2024
}
