#ifndef EXTRADAYS_H
#define EXTRADAYS_H

#include <Arduino.h>

// Jour EXCEPTIONNEL où la sonnerie fonctionne alors qu'elle ne le ferait pas
// normalement (ex : un samedi de cours de rattrapage). Ce jour-là on applique
// l'horaire d'un autre jour de la semaine (tplDow : 0=dimanche ... 6=samedi).
struct ExtraDay {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t tplDow;
};

class ExtraDays {
public:
    static const uint16_t MAX_EXTRAS = 60;

    void begin();
    bool find(uint16_t year, uint8_t month, uint8_t day, uint8_t* tplDow = nullptr) const;
    void replaceAll(const ExtraDay* list, uint16_t n);
    void save();

    uint16_t getCount() const { return _count; }
    const ExtraDay& get(uint16_t idx) const { return _days[idx]; }

private:
    ExtraDay _days[MAX_EXTRAS];
    uint16_t _count = 0;
};

#endif
