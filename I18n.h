#ifndef I18N_H
#define I18N_H

#include <Arduino.h>

enum class Lang : uint8_t { FR = 0, EN = 1 };

// Gestion de la langue d'affichage (persistante en NVS).
namespace I18n {
    void begin();                                   // charge la langue enregistrée
    Lang get();
    void set(Lang l);                               // change ET enregistre
    const char* tr(const char* fr, const char* en); // renvoie le texte dans la langue courante
    const char* dayShort(uint8_t dow);              // 0=dimanche ... 6=samedi
}

#endif
