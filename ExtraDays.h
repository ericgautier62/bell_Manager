#ifndef EXTRADAYS_H
#define EXTRADAYS_H

#include <Arduino.h>

// Horaire personnalisé pour une date exceptionnelle (ou une répétition bornée).
// tplDow conserve les anciennes exceptions qui reprenaient un jour modèle;
// 255 indique qu'il faut utiliser les sonneries personnalisées.
struct ExceptionalBell {
    uint8_t hour;
    uint8_t minute;
    uint8_t durationSeconds;
};

struct ExtraDay {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint16_t endYear;
    uint8_t endMonth;
    uint8_t endDay;
    uint8_t repeatDays; // 0=une fois, 1=quotidien, 7=hebdomadaire
    uint8_t tplDow;
    uint8_t bellCount;
    ExceptionalBell bells[10];
};

class ExtraDays {
public:
    static const uint16_t MAX_EXTRAS = 60;
    static const uint8_t MAX_BELLS = 10;

    void begin();
    const ExtraDay* find(uint16_t year, uint8_t month, uint8_t day) const;
    void replaceAll(const ExtraDay* list, uint16_t n);
    void save();

    uint16_t getCount() const { return _count; }
    const ExtraDay& get(uint16_t idx) const { return _days[idx]; }

private:
    ExtraDay _days[MAX_EXTRAS];
    uint16_t _count = 0;
};

#endif
