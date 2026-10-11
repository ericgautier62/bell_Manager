#include "TimeSync.h"
#include "config.h"
#include <WiFi.h>
#include <time.h>

namespace {
    bool g_pending = false;
    unsigned long g_requestMs = 0;
}

namespace TimeSync {

void request() {
    // Fuseau Cameroun (WAT = UTC+1, sans heure d'été) : getLocalTime() renverra l'heure locale
    configTzTime(TZ_POSIX, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    g_pending = true;
    g_requestMs = millis();
}

bool pending() { return g_pending; }

bool poll(RtcManager& rtc) {
    if (!g_pending) return false;

    if (WiFi.status() != WL_CONNECTED || millis() - g_requestMs > 45000UL) {
        g_pending = false;           // abandon : une nouvelle demande sera refaite plus tard
        return false;
    }

    struct tm t;
    if (!getLocalTime(&t, 0)) return false;   // pas encore reçu (timeout 0 = non bloquant)

    rtc.adjust(DateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec));
    g_pending = false;
    return true;
}

} // namespace TimeSync
