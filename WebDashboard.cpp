#include "WebDashboard.h"
#include "web_ui.h"
#include "logo_web.h"
#include "I18n.h"
#include <WiFi.h>

WebDashboard::WebDashboard(RtcManager& rtc, ScheduleManager& sched, ExceptionCalendar& holidays,
                           ExtraDays& extras, RelayController& relay, bool& timeTrusted)
    : _server(80), _rtc(rtc), _sched(sched), _holidays(holidays),
      _extras(extras), _relay(relay), _trusted(timeTrusted) {}

void WebDashboard::begin() {
    if (_running) return;
    if (!_routesReady) {
        setupRoutes();
        _routesReady = true;
    }
    _server.begin();
    _running = true;
}

void WebDashboard::stop() {
    if (!_running) return;
    _server.stop();
    _running = false;
}

void WebDashboard::handle() {
    if (_running) _server.handleClient();
}

void WebDashboard::setupRoutes() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/logo.png", HTTP_GET, [this]() { handleLogo(); });
    _server.on("/api/state", HTTP_GET, [this]() { handleState(); });
    _server.on("/api/schedules", HTTP_POST, [this]() { handleSchedules(); });
    _server.on("/api/holidays", HTTP_POST, [this]() { handleHolidays(); });
    _server.on("/api/extras", HTTP_POST, [this]() { handleExtras(); });
    _server.on("/api/ring", HTTP_POST, [this]() { handleRing(); });
    _server.on("/api/time", HTTP_POST, [this]() { handleTime(); });
    _server.onNotFound([this]() { _server.send(404, "text/plain", "404"); });
}

void WebDashboard::sendOk() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebDashboard::sendError(const char* msg) {
    _server.send(400, "application/json", String("{\"ok\":false,\"error\":\"") + msg + "\"}");
}

void WebDashboard::handleRoot() {
    _server.sendHeader("Cache-Control", "no-cache");
    _server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void WebDashboard::handleLogo() {
    _server.sendHeader("Cache-Control", "public, max-age=86400");
    _server.send_P(200, "image/png", (PGM_P)LOGO_WEB_PNG, LOGO_WEB_LEN);
}

// ------------------------------------------------------------------
// GET /api/state : tout l'état nécessaire à la page, en un seul JSON.
// Aucune chaîne libre n'y figure (dates, nombres, IP) -> pas d'échappement à gérer.
// ------------------------------------------------------------------
void WebDashboard::handleState() {
    DateTime n = _rtc.now();
    char buf[48];
    String j;
    j.reserve(4096);

    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             n.year(), n.month(), n.day(), n.hour(), n.minute(), n.second());
    j += "{\"now\":\""; j += buf; j += "\"";
    j += ",\"trusted\":"; j += _trusted ? "true" : "false";
    j += ",\"lang\":\""; j += (I18n::get() == Lang::EN) ? "en" : "fr"; j += "\"";
    j += ",\"ip\":\""; j += WiFi.localIP().toString(); j += "\"";
    j += ",\"held\":"; j += _relay.isHeld() ? "true" : "false";

    j += ",\"sched\":[";
    for (uint8_t i = 0; i < _sched.getCount(); i++) {
        const ScheduleEntry& e = _sched.getEntry(i);
        if (i) j += ",";
        snprintf(buf, sizeof(buf), "[%u,%u,%u,%u]", e.hour, e.minute, e.daysMask, e.enabled ? 1 : 0);
        j += buf;
    }
    j += "],\"holidays\":[";
    for (uint16_t i = 0; i < _holidays.getCount(); i++) {
        const ExceptionDate& d = _holidays.getException(i);
        if (i) j += ",";
        snprintf(buf, sizeof(buf), "\"%04u-%02u-%02u\"", d.year, d.month, d.day);
        j += buf;
    }
    j += "],\"extras\":[";
    for (uint16_t i = 0; i < _extras.getCount(); i++) {
        const ExtraDay& d = _extras.get(i);
        if (i) j += ",";
        snprintf(buf, sizeof(buf), "[\"%04u-%02u-%02u\",%u]", d.year, d.month, d.day, d.tplDow);
        j += buf;
    }
    j += "]}";

    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", j);
}

// ------------------------------------------------------------------
// Les POST envoient la liste COMPLÈTE dans l'argument "data" (CSV).
// ------------------------------------------------------------------
void WebDashboard::handleSchedules() {
    if (!_server.hasArg("data")) { sendError("data"); return; }
    String data = _server.arg("data");

    ScheduleEntry list[ScheduleManager::MAX_SCHEDULES];
    uint8_t n = 0;
    int start = 0;
    while (start <= (int)data.length() && n < ScheduleManager::MAX_SCHEDULES) {
        int end = data.indexOf(',', start);
        if (end < 0) end = data.length();
        String tok = data.substring(start, end);
        start = end + 1;
        if (tok.length() == 0) continue;

        int h, m, mask, en;
        if (sscanf(tok.c_str(), "%d:%d:%d:%d", &h, &m, &mask, &en) != 4) { sendError("format"); return; }
        if (h < 0 || h > 23 || m < 0 || m > 59 || mask < 0 || mask > 127) { sendError("range"); return; }
        list[n++] = { (uint8_t)h, (uint8_t)m, (uint8_t)mask, en != 0 };
    }
    _sched.replaceAll(list, n);
    sendOk();
}

void WebDashboard::handleHolidays() {
    if (!_server.hasArg("data")) { sendError("data"); return; }
    String data = _server.arg("data");

    static ExceptionDate list[ExceptionCalendar::MAX_EXCEPTIONS];
    uint16_t n = 0;
    int start = 0;
    while (start <= (int)data.length() && n < ExceptionCalendar::MAX_EXCEPTIONS) {
        int end = data.indexOf(',', start);
        if (end < 0) end = data.length();
        String tok = data.substring(start, end);
        start = end + 1;
        if (tok.length() == 0) continue;

        int y, mo, d;
        if (sscanf(tok.c_str(), "%d-%d-%d", &y, &mo, &d) != 3) { sendError("format"); return; }
        if (y < 2024 || y > 2100 || mo < 1 || mo > 12 || d < 1 || d > 31) { sendError("range"); return; }
        list[n++] = { (uint16_t)y, (uint8_t)mo, (uint8_t)d };
    }
    _holidays.replaceAll(list, n);
    sendOk();
}

void WebDashboard::handleExtras() {
    if (!_server.hasArg("data")) { sendError("data"); return; }
    String data = _server.arg("data");

    ExtraDay list[ExtraDays::MAX_EXTRAS];
    uint16_t n = 0;
    int start = 0;
    while (start <= (int)data.length() && n < ExtraDays::MAX_EXTRAS) {
        int end = data.indexOf(',', start);
        if (end < 0) end = data.length();
        String tok = data.substring(start, end);
        start = end + 1;
        if (tok.length() == 0) continue;

        int y, mo, d, tpl;
        if (sscanf(tok.c_str(), "%d-%d-%d:%d", &y, &mo, &d, &tpl) != 4) { sendError("format"); return; }
        if (y < 2024 || y > 2100 || mo < 1 || mo > 12 || d < 1 || d > 31 || tpl < 0 || tpl > 6) { sendError("range"); return; }
        list[n++] = { (uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)tpl };
    }
    _extras.replaceAll(list, n);
    sendOk();
}

// POST /api/ring  on=1 : appui maintenu (répété toutes les 400 ms par la page)
//                 on=0 : relâché. Le watchdog du RelayController coupe tout seul
//                        si les "on=1" cessent d'arriver (Wi-Fi perdu, page fermée).
void WebDashboard::handleRing() {
    if (_server.arg("on") == "1") {
        _relay.holdOn();      // idempotent : ne fait que rafraîchir le heartbeat si déjà actif
    } else {
        _relay.holdOff();
    }
    sendOk();
}

// POST /api/time  data=YYYY-MM-DDTHH:MM:SS (heure locale envoyée par le navigateur)
void WebDashboard::handleTime() {
    if (!_server.hasArg("data")) { sendError("data"); return; }
    int y, mo, d, h, mi, s;
    if (sscanf(_server.arg("data").c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) { sendError("format"); return; }
    if (y < 2024 || y > 2100 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) {
        sendError("range");
        return;
    }
    _rtc.adjust(DateTime(y, mo, d, h, mi, s));
    _trusted = true;
    sendOk();
}
