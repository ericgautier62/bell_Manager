#ifndef MENUMANAGER_H
#define MENUMANAGER_H

#include <vector>
#include <functional>
#include "Menu.h"
#include "DisplayManager.h"
#include "Encoder.h"

class MenuManager {
public:
    MenuManager(DisplayManager& display, Encoder& encoder);
    void pushMenu(Menu* menu);
    void popMenu();
    void update();

    // Tâche exécutée en continu, y compris PENDANT les animations de veille :
    // serveur web, horaires de sonnerie, Wi-Fi... (sans elle, tout s'arrêterait
    // dès que l'écran passe en veille).
    void setBackgroundTask(std::function<void()> task) { _background = task; }

private:
    DisplayManager& _display;
    Encoder& _encoder;
    std::vector<Menu*> _menuStack;
    Menu* _currentMenu;
    std::function<void()> _background;

    unsigned long _lastActivityMs; // pour déclencher l'animation d'inactivité
};

#endif
