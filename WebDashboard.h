#ifndef WEBDASHBOARD_H
#define WEBDASHBOARD_H

#include <Arduino.h>
#include <WebServer.h>
#include "RtcManager.h"
#include "ScheduleManager.h"
#include "ExceptionCalendar.h"
#include "ExtraDays.h"
#include "RelayController.h"

// Serveur web embarqué (bibliothèque WebServer intégrée à l'ESP32 : aucune
// dépendance à installer). Sert la page d'administration et une petite API.
class WebDashboard {
public:
    WebDashboard(RtcManager& rtc, ScheduleManager& sched, ExceptionCalendar& holidays,
                 ExtraDays& extras, RelayController& relay, bool& timeTrusted);

    void begin();          // démarre (idempotent)
    void stop();           // libère le port 80 (nécessaire avant le portail Wi-Fi)
    void handle();         // à appeler très souvent
    bool running() const { return _running; }

private:
    WebServer _server;
    RtcManager& _rtc;
    ScheduleManager& _sched;
    ExceptionCalendar& _holidays;
    ExtraDays& _extras;
    RelayController& _relay;
    bool& _trusted;
    bool _running = false;
    bool _routesReady = false;

    void setupRoutes();
    void handleRoot();
    void handleLogo();
    void handleState();
    void handleSchedules();
    void handleHolidays();
    void handleExtras();
    void handleRing();
    void handleTime();
    void sendOk();
    void sendError(const char* msg);
};

#endif
