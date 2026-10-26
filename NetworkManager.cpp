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
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            Serial.printf("[WiFi] STA deconnectee, raison=%u\n",
                          info.wifi_sta_disconnected.reason);
        }
    });
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
    // Empêche une reconnexion automatique de l'ancien réseau de chevaucher
    // la tentative vers le réseau choisi dans le portail.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false); // coupe la STA sans effacer les identifiants
    delay(500);

    _wm = new WiFiManager();
    _wm->setConfigPortalBlocking(false);          // on garde la main (écran, horaires, relais...)
    _wm->setConfigPortalTimeout(PORTAL_TIMEOUT_S);
    _wm->setConnectTimeout(20);
    _wm->setCleanConnect(true);
    _wm->setTitle("KAIROSLAB");
    // En mode non bloquant, WiFiManager renvoie false immédiatement (aucune
    // connexion n'a encore été saisie), même si le portail démarre correctement.
    _wm->startConfigPortal(AP_NAME);
    _portalStartMs = millis();
    _portalConnectionStatus = 0;
    WiFiMode_t mode = WiFi.getMode();
    String apSsid = WiFi.softAPSSID();
    IPAddress apIp = WiFi.softAPIP();
    bool apReady = _wm->getConfigPortalActive() && (mode & WIFI_AP) &&
                   apSsid == AP_NAME && apIp != IPAddress(0, 0, 0, 0);
    Serial.printf("[AP] active=%d mode=%u ssid='%s' ip=%s clients=%u\n",
                  _wm->getConfigPortalActive(), (unsigned)mode, apSsid.c_str(),
                  apIp.toString().c_str(), (unsigned)WiFi.softAPgetStationNum());
    if (!apReady) {
        Serial.println("[AP] echec: mode/SSID/IP du point d'acces invalide");
        stopPortal();
        return false;
    }
    return true;
}

bool NetworkManager::processPortal() {
    if (!_wm) return false;
    bool connected = _wm->process();
    _portalConnectionStatus = _wm->getLastConxResult();
    if (connected) _portalConnectionStatus = WL_CONNECTED;
    return connected;
}

bool NetworkManager::portalActive() const {
    return _wm && _wm->getConfigPortalActive();
}

uint8_t NetworkManager::portalConnectionStatus() const {
    return _portalConnectionStatus;
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
    WiFi.setAutoReconnect(true);
    // WiFiManager relance déjà la STA lors de la fermeture du portail quand
    // son état est idle. Ne pas appeler begin() une seconde fois ici.
    _wasConnected = false;              // service() relancera serveur web + NTP
}
