#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <Arduino.h>

// daysMask : un bit par jour de la semaine, convention RTClib
// DateTime::dayOfTheWeek() -> 0=dimanche, 1=lundi, ..., 6=samedi.
// Exemple "lundi à vendredi" : (1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5) = 0b0111110
struct ScheduleEntry {
    uint8_t hour;
    uint8_t minute;
    uint8_t daysMask;
    bool enabled;
};

#endif
