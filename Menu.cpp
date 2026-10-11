#include "Menu.h"

// ---- MenuItem ----
MenuItem::MenuItem(const char* nameFr, const char* nameEn, Menu* subMenu, void (*action)())
    : _nameFr(nameFr), _nameEn(nameEn), _subMenu(subMenu), _action(action), _isBack(false) {}

MenuItem MenuItem::createBackItem(const char* nameFr, const char* nameEn) {
    MenuItem m(nameFr, nameEn);
    m._isBack = true;
    return m;
}

const char* MenuItem::getName() const { return I18n::tr(_nameFr, _nameEn); }
Menu* MenuItem::getSubMenu() const { return _subMenu; }
bool MenuItem::hasSubMenu() const { return _subMenu != nullptr; }
bool MenuItem::isBackItem() const { return _isBack; }
void MenuItem::execute() const { if (_action) _action(); }

// ---- Menu ----
Menu::Menu(const char* titleFr, const char* titleEn)
    : _titleFr(titleFr), _titleEn(titleEn), _selected(0) {}

void Menu::addItem(const MenuItem& item) { _items.push_back(item); }

void Menu::selectNext() {
    if (!_items.empty()) _selected = (_selected + 1) % _items.size();
}

void Menu::selectPrev() {
    if (!_items.empty()) _selected = (_selected + _items.size() - 1) % _items.size();
}

const MenuItem& Menu::getSelectedItem() const { return _items[_selected]; }
size_t Menu::getItemCount() const { return _items.size(); }
const MenuItem& Menu::getItem(size_t idx) const { return _items[idx]; }
size_t Menu::getSelectedIndex() const { return _selected; }

const char* Menu::getTitle() const {
    if (!_titleFr) return nullptr;
    return I18n::tr(_titleFr, _titleEn ? _titleEn : _titleFr);
}
