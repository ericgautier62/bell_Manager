#include "WebDashboard.h"
#include "web_ui.h"
#include "logo_web.h"
#include "I18n.h"
#include <WiFi.h>

namespace {
bool validCalendarDate(int y, int month, int day) {
    if (y < 2024 || y > 2100 || month < 1 || month > 12 || day < 1) return false;
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int maxDay = days[month - 1];
    if (month == 2 && (y % 400 == 0 || (y % 4 == 0 && y % 100 != 0))) maxDay = 29;
    return day <= maxDay;
}
int32_t dateKey(int y, int month, int day) { return y * 10000L + month * 100L + day; }

String formValue(const String& body, const char* key) {
    String prefix = String(key) + "=";
    int start = body.indexOf(prefix);
    if (start < 0) return String();
    start += prefix.length();
    int end = body.indexOf('&', start);
    if (end < 0) end = body.length();
    String encoded = body.substring(start, end), decoded;
    decoded.reserve(encoded.length());
    for (int i = 0; i < encoded.length(); ++i) {
        char c = encoded[i];
        if (c == '+' ) decoded += ' ';
        else if (c == '%' && i + 2 < encoded.length()) {
            char hex[3] = { encoded[i + 1], encoded[i + 2], 0 };
            decoded += (char)strtol(hex, nullptr, 16);
            i += 2;
        } else decoded += c;
    }
    return decoded;
}
}

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
    _server.on("/api/settings", HTTP_POST, [this]() { handleSettings(); });
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
String WebDashboard::stateJson() {
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
        snprintf(buf, sizeof(buf), "[%u,%u,%u,%u,%u]", e.hour, e.minute, e.daysMask, e.enabled ? 1 : 0, e.durationSeconds);
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
        snprintf(buf, sizeof(buf), "[\"%04u-%02u-%02u\",\"%04u-%02u-%02u\",%u,%u,[",
                 d.year, d.month, d.day, d.endYear, d.endMonth, d.endDay, d.repeatDays, d.tplDow);
        j += buf;
        for (uint8_t b = 0; b < d.bellCount; ++b) {
            if (b) j += ",";
            snprintf(buf, sizeof(buf), "[%u,%u,%u]", d.bells[b].hour, d.bells[b].minute, d.bells[b].durationSeconds);
            j += buf;
        }
        j += "]]";
    }
    j += "]}";

    return j;
}

void WebDashboard::handleState() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", stateJson());
}

String WebDashboard::processApiRequest(const String& method, const String& path, const String& body) {
    if (method == "GET" && path == "/api/state") return stateJson();
    if (method == "POST" && path == "/api/ring") {
        if (formValue(body, "on") == "1") _relay.holdOn();
        else _relay.holdOff();
        return "{\"ok\":true}";
    }
    if (method == "POST" && path == "/api/time") {
        String value = formValue(body, "data");
        int y, mo, d, h, mi, s;
        if (sscanf(value.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6 ||
            y < 2024 || y > 2100 || !validCalendarDate(y, mo, d) || h < 0 || h > 23 ||
            mi < 0 || mi > 59 || s < 0 || s > 59) return "{\"ok\":false,\"error\":\"time_format\"}";
        _rtc.adjust(DateTime(y, mo, d, h, mi, s));
        _trusted = true;
        return "{\"ok\":true}";
    }
    if (method != "POST" || path != "/api/settings") return "{\"ok\":false,\"error\":\"not_found\"}";

    String schedData = formValue(body, "sched");
    String holData = formValue(body, "hol");
    String extData = formValue(body, "ext");
    if (body.indexOf("sched=") < 0 || body.indexOf("hol=") < 0 || body.indexOf("ext=") < 0)
        return "{\"ok\":false,\"error\":\"fields\"}";
    static ScheduleEntry schedules[ScheduleManager::MAX_SCHEDULES];
    static ExceptionDate holidays[ExceptionCalendar::MAX_EXCEPTIONS];
    static ExtraDay extras[ExtraDays::MAX_EXTRAS];
    uint8_t ns = 0; uint16_t nh = 0, ne = 0;
    int start = 0;
    while (start < (int)schedData.length()) {
        if (ns >= ScheduleManager::MAX_SCHEDULES) return "{\"ok\":false,\"error\":\"sched_limit\"}";
        int end = schedData.indexOf(',', start); if (end < 0) end = schedData.length();
        String tok = schedData.substring(start, end); start = end + 1;
        int h, m, mask, enabled, duration;
        if (sscanf(tok.c_str(), "%d:%d:%d:%d:%d", &h, &m, &mask, &enabled, &duration) != 5 ||
            h < 0 || h > 23 || m < 0 || m > 59 || mask < 0 || mask > 127 ||
            (enabled != 0 && enabled != 1) || duration < 1 || duration > 60)
            return "{\"ok\":false,\"error\":\"sched_format\"}";
        schedules[ns++] = { (uint8_t)h, (uint8_t)m, (uint8_t)mask, enabled != 0, (uint8_t)duration };
    }
    start = 0;
    while (start < (int)holData.length()) {
        if (nh >= ExceptionCalendar::MAX_EXCEPTIONS) return "{\"ok\":false,\"error\":\"hol_limit\"}";
        int end = holData.indexOf(',', start); if (end < 0) end = holData.length();
        String tok = holData.substring(start, end); start = end + 1;
        int y, mo, d;
        if (sscanf(tok.c_str(), "%d-%d-%d", &y, &mo, &d) != 3 || !validCalendarDate(y, mo, d))
            return "{\"ok\":false,\"error\":\"hol_format\"}";
        holidays[nh++] = { (uint16_t)y, (uint8_t)mo, (uint8_t)d };
    }
    start = 0;
    while (start < (int)extData.length()) {
        if (ne >= ExtraDays::MAX_EXTRAS) return "{\"ok\":false,\"error\":\"ext_limit\"}";
        int end = extData.indexOf(',', start); if (end < 0) end = extData.length();
        String tok = extData.substring(start, end); start = end + 1;
        int y, mo, d, ey, em, ed, repeat, tpl, consumed = 0;
        if (sscanf(tok.c_str(), "%d-%d-%d:%d-%d-%d:%d:%d:%n", &y, &mo, &d, &ey, &em, &ed, &repeat, &tpl, &consumed) != 8 ||
            !validCalendarDate(y, mo, d) || !validCalendarDate(ey, em, ed) ||
            dateKey(ey, em, ed) < dateKey(y, mo, d) ||
            (repeat != 0 && repeat != 1 && repeat != 7) || (tpl != 255 && (tpl < 0 || tpl > 6)))
            return "{\"ok\":false,\"error\":\"ext_format\"}";
        ExtraDay plan{};
        plan.year = y; plan.month = mo; plan.day = d;
        plan.endYear = ey; plan.endMonth = em; plan.endDay = ed;
        plan.repeatDays = repeat; plan.tplDow = tpl;
        String bells = tok.substring(consumed);
        int bs = 0;
        while (bs < (int)bells.length()) {
            if (plan.bellCount >= ExtraDays::MAX_BELLS) return "{\"ok\":false,\"error\":\"bell_limit\"}";
            int be = bells.indexOf('/', bs); if (be < 0) be = bells.length();
            String bell = bells.substring(bs, be); bs = be + 1;
            int bh, bm, duration;
            if (sscanf(bell.c_str(), "%d-%d-%d", &bh, &bm, &duration) != 3 ||
                bh < 0 || bh > 23 || bm < 0 || bm > 59 || duration < 1 || duration > 60)
                return "{\"ok\":false,\"error\":\"bell_format\"}";
            plan.bells[plan.bellCount++] = { (uint8_t)bh, (uint8_t)bm, (uint8_t)duration };
        }
        if (tpl == 255 && plan.bellCount == 0) return "{\"ok\":false,\"error\":\"bell_required\"}";
        for (uint16_t i = 0; i < ne; ++i) {
            const ExtraDay& prior = extras[i];
            if (dateKey(y, mo, d) <= dateKey(prior.endYear, prior.endMonth, prior.endDay) &&
                dateKey(prior.year, prior.month, prior.day) <= dateKey(ey, em, ed))
                return "{\"ok\":false,\"error\":\"ext_overlap\"}";
        }
        extras[ne++] = plan;
    }
    _sched.replaceAll(schedules, ns);
    _holidays.replaceAll(holidays, nh);
    _extras.replaceAll(extras, ne);
    return "{\"ok\":true}";
}

// ------------------------------------------------------------------
// Les POST envoient la liste COMPLÈTE dans l'argument "data" (CSV).
// ------------------------------------------------------------------
// Un seul POST applique les trois catégories après validation complète.
void WebDashboard::handleSettings() {
    if (!_server.hasArg("sched") || !_server.hasArg("hol") || !_server.hasArg("ext")) {
        sendError("fields"); return;
    }
    String schedData = _server.arg("sched");
    String holData = _server.arg("hol");
    String extData = _server.arg("ext");
    static ScheduleEntry schedules[ScheduleManager::MAX_SCHEDULES];
    static ExceptionDate holidays[ExceptionCalendar::MAX_EXCEPTIONS];
    static ExtraDay extras[ExtraDays::MAX_EXTRAS];
    uint8_t ns = 0; uint16_t nh = 0, ne = 0;
    int start = 0;
    while (start < (int)schedData.length()) {
        if (ns >= ScheduleManager::MAX_SCHEDULES) { sendError("sched_limit"); return; }
        int end = schedData.indexOf(',', start); if (end < 0) end = schedData.length();
        String tok = schedData.substring(start, end); start = end + 1;
        int h, m, mask, en, duration;
        if (sscanf(tok.c_str(), "%d:%d:%d:%d:%d", &h, &m, &mask, &en, &duration) != 5 || h < 0 || h > 23 || m < 0 || m > 59 || mask < 0 || mask > 127 || (en != 0 && en != 1) || duration < 1 || duration > 60) { sendError("sched_format"); return; }
        schedules[ns++] = { (uint8_t)h, (uint8_t)m, (uint8_t)mask, en != 0, (uint8_t)duration };
    }
    start = 0;
    while (start < (int)holData.length()) {
        if (nh >= ExceptionCalendar::MAX_EXCEPTIONS) { sendError("hol_limit"); return; }
        int end = holData.indexOf(',', start); if (end < 0) end = holData.length();
        String tok = holData.substring(start, end); start = end + 1;
        int y, mo, d;
        if (sscanf(tok.c_str(), "%d-%d-%d", &y, &mo, &d) != 3 || y < 2024 || y > 2100 || mo < 1 || mo > 12 || d < 1 || d > 31) { sendError("hol_format"); return; }
        holidays[nh++] = { (uint16_t)y, (uint8_t)mo, (uint8_t)d };
    }
    start = 0;
    while (start < (int)extData.length()) {
        if (ne >= ExtraDays::MAX_EXTRAS) { sendError("ext_limit"); return; }
        int end = extData.indexOf(',', start); if (end < 0) end = extData.length();
        String tok = extData.substring(start, end); start = end + 1;
        int y, mo, d, ey, em, ed, repeat, tpl, consumed = 0;
        if (sscanf(tok.c_str(), "%d-%d-%d:%d-%d-%d:%d:%d:%n", &y, &mo, &d, &ey, &em, &ed, &repeat, &tpl, &consumed) != 8 ||
            !validCalendarDate(y, mo, d) || !validCalendarDate(ey, em, ed) ||
            dateKey(ey, em, ed) < dateKey(y, mo, d) ||
            (repeat != 0 && repeat != 1 && repeat != 7) || (tpl != 255 && (tpl < 0 || tpl > 6))) { sendError("ext_format"); return; }
        ExtraDay plan{};
        plan.year = y; plan.month = mo; plan.day = d;
        plan.endYear = ey; plan.endMonth = em; plan.endDay = ed;
        plan.repeatDays = repeat; plan.tplDow = tpl;
        String bells = tok.substring(consumed);
        int bs = 0;
        while (bs < (int)bells.length()) {
            if (plan.bellCount >= ExtraDays::MAX_BELLS) { sendError("bell_limit"); return; }
            int be = bells.indexOf('/', bs); if (be < 0) be = bells.length();
            String bell = bells.substring(bs, be); bs = be + 1;
            int bh, bm, duration;
            if (sscanf(bell.c_str(), "%d-%d-%d", &bh, &bm, &duration) != 3 ||
                bh < 0 || bh > 23 || bm < 0 || bm > 59 || duration < 1 || duration > 60) { sendError("bell_format"); return; }
            plan.bells[plan.bellCount++] = { (uint8_t)bh, (uint8_t)bm, (uint8_t)duration };
        }
        if (tpl == 255 && plan.bellCount == 0) { sendError("bell_required"); return; }
        for (uint16_t i = 0; i < ne; ++i) {
            const ExtraDay& prior = extras[i];
            if (dateKey(y, mo, d) <= dateKey(prior.endYear, prior.endMonth, prior.endDay) &&
                dateKey(prior.year, prior.month, prior.day) <= dateKey(ey, em, ed)) {
                sendError("ext_overlap"); return;
            }
        }
        extras[ne++] = plan;
    }
    _sched.replaceAll(schedules, ns);
    _holidays.replaceAll(holidays, nh);
    _extras.replaceAll(extras, ne);
    sendOk();
}

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

        int h, m, mask, en, duration = 2;
        int fields = sscanf(tok.c_str(), "%d:%d:%d:%d:%d", &h, &m, &mask, &en, &duration);
        if (fields == 4) duration = 2;
        else if (fields != 5) { sendError("format"); return; }
        if (h < 0 || h > 23 || m < 0 || m > 59 || mask < 0 || mask > 127 || (en != 0 && en != 1) || duration < 1 || duration > 60) { sendError("range"); return; }
        list[n++] = { (uint8_t)h, (uint8_t)m, (uint8_t)mask, en != 0, (uint8_t)duration };
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

void WebDashboard::handleExtras() { sendError("use_settings"); }

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
