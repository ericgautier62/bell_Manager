#ifndef SCREENS_H
#define SCREENS_H

#include <Arduino.h>
#include <functional>
#include "DisplayManager.h"
#include "Encoder.h"
#include "RelayController.h"
#include "RtcManager.h"
#include "NetworkManager.h"

// Écrans "plein écran" (modaux) ouverts depuis les menus. Chacun tourne dans sa
// propre boucle mais appelle en permanence la tâche de fond (serveur web,
// horaires, relais...) via pump() : rien ne se fige pendant qu'ils sont ouverts.
class Screens {
public:
    Screens(DisplayManager& display, Encoder& encoder, RelayController& relay,
            RtcManager& rtc, NetworkManager& net, bool& timeTrusted,
            std::function<void()> backgroundTick);

    void ringNow();         // Sonnerie > Sonner maintenant (symbole power, maintien)
    void status();          // Sonnerie > Statut / heure
    void wifi();            // Connexion > Wi-Fi (adresse web OU portail de configuration)
    void dateTimeEditor();  // Réglage > Heure/date
    void webInfo();         // Sonnerie > Horaires / Vacances : rappelle l'adresse de l'interface web
    void toast(const char* line1, const char* line2 = nullptr, uint16_t ms = 1500);

private:
    DisplayManager& _d;
    Encoder& _enc;
    RelayController& _relay;
    RtcManager& _rtc;
    NetworkManager& _net;
    bool& _trusted;
    std::function<void()> _tick;

    void pump();
    void waitRelease();
    bool wifiInfo();        // true si l'utilisateur demande un autre réseau (appui long)
    void portalFlow();
};

#endif
