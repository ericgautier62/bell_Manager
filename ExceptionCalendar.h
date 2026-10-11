#ifndef EXCEPTIONCALENDAR_H
#define EXCEPTIONCALENDAR_H

#include <Arduino.h>

struct ExceptionDate {
    uint16_t year;
    uint8_t month;
    uint8_t day;
};

// Jours SANS sonnerie (fériés, vacances).
class ExceptionCalendar {
public:
    static const uint16_t MAX_EXCEPTIONS = 200;

    void begin();                                   // charge depuis la NVS
    bool isException(uint16_t year, uint8_t month, uint8_t day) const;
    bool addException(uint16_t year, uint8_t month, uint8_t day);
    void replaceAll(const ExceptionDate* list, uint16_t n);   // remplace toute la liste + sauvegarde
    void save();

    uint16_t getCount() const { return _count; }
    const ExceptionDate& getException(uint16_t idx) const { return _dates[idx]; }

private:
    ExceptionDate _dates[MAX_EXCEPTIONS];
    uint16_t _count = 0;
};

#endif
