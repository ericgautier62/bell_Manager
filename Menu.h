#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <vector>
#include "I18n.h"

class Menu; // déclaration anticipée

// Chaque entrée porte son nom en français ET en anglais : le texte affiché
// est choisi à chaque rafraîchissement selon la langue courante, donc un
// changement de langue s'applique immédiatement à tous les menus.
class MenuItem {
public:
    MenuItem(const char* nameFr, const char* nameEn, Menu* subMenu = nullptr, void (*action)() = nullptr);

    // Item spécial "retour" : dépile le menu courant (popMenu()).
    static MenuItem createBackItem(const char* nameFr = "Retour", const char* nameEn = "Back");

    const char* getName() const;
    Menu* getSubMenu() const;
    bool hasSubMenu() const;
    bool isBackItem() const;
    void execute() const;

private:
    const char* _nameFr;
    const char* _nameEn;
    Menu* _subMenu;
    void (*_action)();
    bool _isBack;
};

class Menu {
public:
    Menu(const char* titleFr = nullptr, const char* titleEn = nullptr);
    void addItem(const MenuItem& item);
    void selectNext();
    void selectPrev();
    const MenuItem& getSelectedItem() const;
    size_t getItemCount() const;
    const MenuItem& getItem(size_t idx) const;
    size_t getSelectedIndex() const;
    const char* getTitle() const;

private:
    const char* _titleFr;
    const char* _titleEn;
    std::vector<MenuItem> _items;
    size_t _selected;
};

#endif
