#include "NetworkManager.h"
#include "config.h"
#include "TimeSync.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>

NetworkManager::NetworkManager(WebDashboard& web, RtcManager& rtc, bool& timeTrusted)
    : _web(web), _rtc(rtc), _trusted(timeTrusted) {}

void NetworkManager::begin() {
    WiFi.persistent(true);              // les identifiants restent en NVS (survivent aux redémarrages)
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);               // serveur web plus réactif
    WiFi.begin();                       // sans argument : réseau mémorisé, s'il existe
}

bool NetworkManager::isConnected() const { return WiFi.status() == WL_CONNECTED; }
String NetworkManager::ip() const { return WiFi.localIP().toString(); }
String NetworkManager::ssid() const { return WiFi.SSID(); }

bool NetworkManager::hasSavedNetwork() const {
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) return false;
    return conf.sta.ssid[0] != 0;
}

void NetworkManager::service() {
    if (_wm) return;   // portail ouvert : on ne touche à rien (le port 80 lui appartient)

    bool conn = isConnected();

    if (conn && !_wasConnected) {
        _wasConnected = true;
        _web.begin();                              // serveur web dès que le réseau est là
        if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
        TimeSync::request();
    } else if (!conn) {
        _wasConnected = false;
    }

    if (conn) {
        if (TimeSync::poll(_rtc)) {
            _trusted = true;
            _everSynced = true;
            _lastSyncMs = millis();
        }
        // Resynchronisation périodique pour corriger la dérive du RTC
        if (_everSynced && !TimeSync::pending() && millis() - _lastSyncMs > NTP_RESYNC_MS) {
            TimeSync::request();
            _lastSyncMs = millis();
        }
        _web.handle();
    }
}

bool NetworkManager::startPortal() {
    if (_wm) return true;

    _web.stop();                  // libère le port 80 pour le portail
    MDNS.end();
    WiFi.disconnect();            // sans effacer les identifiants mémorisés
    delay(100);

    _wm = new WiFiManager();
    _wm->setConfigPortalBlocking(false);          // on garde la main (écran, horaires, relais...)
    _wm->setConfigPortalTimeout(PORTAL_TIMEOUT_S);
    _wm->setTitle("KAIROSLAB");
    bool ok = _wm->startConfigPortal(AP_NAME);
    _portalStartMs = millis();
    return ok;
}

void NetworkManager::processPortal() {
    if (_wm) _wm->process();
}

bool NetworkManager::portalActive() const {
    return _wm && _wm->getConfigPortalActive();
}

unsigned long NetworkManager::portalElapsedMs() const {
    return _wm ? millis() - _portalStartMs : 0;
}

void NetworkManager::stopPortal() {
    if (_wm) {
        if (_wm->getConfigPortalActive()) _wm->stopConfigPortal();
        delete _wm;
        _wm = nullptr;
    }
    WiFi.mode(WIFI_STA);
    if (!isConnected()) WiFi.begin();   // reprend le réseau mémorisé si on a annulé
    _wasConnected = false;              // service() relancera serveur web + NTP
}
