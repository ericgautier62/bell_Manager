#include "I18n.h"
#include <Preferences.h>

namespace {
    Lang g_lang = Lang::FR;
    const char* const DAYS_FR[7] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
    const char* const DAYS_EN[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
}

namespace I18n {

void begin() {
    Preferences p;
    p.begin("settings", false);
    uint8_t v = p.getUChar("lang", 0);
    p.end();
    g_lang = (v == 1) ? Lang::EN : Lang::FR;
}

Lang get() { return g_lang; }

void set(Lang l) {
    g_lang = l;
    Preferences p;
    p.begin("settings", false);
    p.putUChar("lang", (uint8_t)l);
    p.end();
}

const char* tr(const char* fr, const char* en) {
    if (g_lang == Lang::EN && en) return en;
    return fr;
}

const char* dayShort(uint8_t dow) {
    if (dow > 6) dow = 0;
    return (g_lang == Lang::EN) ? DAYS_EN[dow] : DAYS_FR[dow];
}

} // namespace I18n
