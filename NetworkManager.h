#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <Arduino.h>
#include "WebDashboard.h"
#include "RtcManager.h"

class WiFiManager;   // déclaration anticipée (inclus seulement dans le .cpp)

// Gère le Wi-Fi en arrière-plan : reconnexion automatique au réseau mémorisé,
// démarrage du serveur web, synchro NTP, et portail de configuration (hotspot).
class NetworkManager {
public:
    NetworkManager(WebDashboard& web, RtcManager& rtc, bool& timeTrusted);

    void begin();      // lance la connexion au réseau mémorisé (non bloquant)
    void service();    // à appeler en continu (via la tâche de fond)

    bool isConnected() const;
    String ip() const;
    String ssid() const;
    bool hasSavedNetwork() const;      // un réseau est-il mémorisé en NVS ?

    // --- Portail de configuration (hotspot "KairosLab-Setup") ---
    bool startPortal();                // true si le hotspot a démarré
    void processPortal();              // à appeler en boucle tant que le portail est ouvert
    bool portalActive() const;
    unsigned long portalElapsedMs() const;
    void stopPortal();                 // ferme le portail et reprend la connexion normale

private:
    WebDashboard& _web;
    RtcManager& _rtc;
    bool& _trusted;

    WiFiManager* _wm = nullptr;
    unsigned long _portalStartMs = 0;
    bool _wasConnected = false;
    unsigned long _lastSyncMs = 0;
    bool _everSynced = false;
};

#endif
