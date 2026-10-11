#include "MenuManager.h"
#include "config.h"

MenuManager::MenuManager(DisplayManager& display, Encoder& encoder)
    : _display(display), _encoder(encoder), _currentMenu(nullptr), _lastActivityMs(0) {}

void MenuManager::pushMenu(Menu* menu) {
    if (menu) {
        _menuStack.push_back(menu);
        _currentMenu = menu;
        _encoder.resetCounter();
        _lastActivityMs = millis();
    }
}

void MenuManager::popMenu() {
    if (_menuStack.size() > 1) {
        _menuStack.pop_back();
        _currentMenu = _menuStack.back();
        _encoder.resetCounter();
        _lastActivityMs = millis();
    }
}

void MenuManager::update() {
    if (!_currentMenu) return;

    if (_background) _background();

    bool activity = false;

    // On accumule le delta : avec le décodage par interruption, plusieurs
    // crans peuvent s'être produits entre deux appels à update().
    int delta = _encoder.getCounter();
    if (delta > 0) {
        for (int i = 0; i < delta; i++) _currentMenu->selectNext();
        _encoder.resetCounter();
        activity = true;
    } else if (delta < 0) {
        for (int i = 0; i < -delta; i++) _currentMenu->selectPrev();
        _encoder.resetCounter();
        activity = true;
    }

    if (_encoder.isButtonPressed()) {
        activity = true;
        const MenuItem& item = _currentMenu->getSelectedItem();
        if (item.isBackItem()) {
            popMenu();
        } else if (item.hasSubMenu()) {
            pushMenu(item.getSubMenu());
        } else {
            // Les actions gèrent elles-mêmes leur affichage (écrans dédiés,
            // messages...). Plus de message générique ni de delay() bloquant ici.
            item.execute();
            _encoder.resetCounter();
            _encoder.clearButtonEvent();
        }
    }

    if (activity) {
        _lastActivityMs = millis();
    } else if (millis() - _lastActivityMs > IDLE_TIMEOUT_MS) {
        // Mode veille : animations aléatoires (dont l'horloge) tant qu'aucune
        // interaction n'est détectée. shouldAbort() est appelée à chaque frame :
        // elle fait avancer le debounce du bouton ET la tâche de fond.
        Encoder* enc = &_encoder;
        std::function<void()> bg = _background;
        auto shouldAbort = [enc, bg]() {
            enc->update();
            if (bg) bg();
            return enc->hasPendingActivity();
        };

        do {
            _display.playIdleAnimation(shouldAbort);
        } while (!shouldAbort());

        _encoder.resetCounter();
        _lastActivityMs = millis();
    }

    _display.displayMenu(*_currentMenu);
}
