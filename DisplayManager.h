#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <U8g2lib.h>
#include <functional>

// Heure fournie par le sketch (RTC) pour l'animation d'horloge de veille.
struct ClockInfo {
    bool valid = false;
    uint8_t hour = 0, minute = 0, second = 0;
    uint8_t day = 1, month = 1, dow = 0;   // dow : 0=dimanche
    uint16_t year = 2026;
};

class Menu; // déclaration anticipée

class DisplayManager {
public:
    DisplayManager(U8G2& u8g2);
    void begin();
    void displayMenu(const Menu& menu);
    void displayMessage(const char* msg);
    void displayMessage(const char* line1, const char* line2);

    // Accès direct à U8g2 pour les écrans dédiés (sonnerie, Wi-Fi, éditeur d'heure...).
    U8G2& raw() { return _u8g2; }

    // Source de l'heure pour l'animation d'horloge.
    void setClockProvider(std::function<ClockInfo()> provider) { _clock = provider; }

    // Petite animation "bonhomme en brindille qui court" à afficher
    // une fois au démarrage, avant setupMenus().
    void playStartupAnimation();

    // Écran de démarrage avec le logo KAIROSLAB (marque "K" + texte en
    // police système, car le texte du logo original n'est pas lisible
    // en bitmap à cette résolution).
    void playLogoSplash();

    // Animation de veille : joue une animation aléatoire parmi celles
    // ci-dessous. shouldAbort() est vérifié à chaque frame et permet
    // d'interrompre l'animation dès qu'une interaction est détectée.
    void playIdleAnimation(const std::function<bool()>& shouldAbort);

private:
    U8G2& _u8g2;
    std::function<ClockInfo()> _clock;

    void drawStickFigure(int x, int y, bool poseA);

    // Animations d'inactivité
    void idleAnimSleep(const std::function<bool()>& shouldAbort);   // bonhomme qui dort, "z" flottants
    void idleAnimBounce(const std::function<bool()>& shouldAbort);  // balle qui rebondit sur les bords
    void idleAnimWave(const std::function<bool()>& shouldAbort);    // bonhomme qui salue de la main
    void idleAnimLogoBounce(const std::function<bool()>& shouldAbort); // logo KAIROSLAB qui rebondit
    void idleAnimClock(const std::function<bool()>& shouldAbort);      // horloge analogique + numérique
};

#endif
