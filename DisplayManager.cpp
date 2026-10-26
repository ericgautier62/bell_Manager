#include "DisplayManager.h"
#include "Menu.h"
#include "logo_bitmap.h"
#include "I18n.h"
#include <math.h>

DisplayManager::DisplayManager(U8G2& u8g2) : _u8g2(u8g2) {}

void DisplayManager::begin() {
    _u8g2.begin();
    _u8g2.enableUTF8Print();          // accents (é, è, à...) dans les textes
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.setFontPosTop();
}

void DisplayManager::setPowerSave(bool enabled) {
    _u8g2.setPowerSave(enabled ? 1 : 0);
}

void DisplayManager::displayMenu(const Menu& menu) {
    const size_t VISIBLE_ITEMS = 4; // 20,31,42,53 px -> tient dans 64px de haut
    size_t total = menu.getItemCount();
    size_t selected = menu.getSelectedIndex();

    // Fenêtre de défilement centrée autour de la sélection
    size_t start = 0;
    if (total > VISIBLE_ITEMS) {
        if (selected >= VISIBLE_ITEMS - 1) {
            start = selected - (VISIBLE_ITEMS - 2);
        }
        size_t maxStart = total - VISIBLE_ITEMS;
        if (start > maxStart) start = maxStart;
    }

    _u8g2.firstPage();
    do {
        _u8g2.setCursor(0, 0);
        _u8g2.print(menu.getTitle() ? menu.getTitle() : "Menu");
        _u8g2.drawHLine(0, 10, _u8g2.getWidth());

        int y = 20;
        for (size_t i = start; i < total && i < start + VISIBLE_ITEMS; i++) {
            _u8g2.setCursor(10, y);
            _u8g2.print(i == selected ? "> " : "  ");
            _u8g2.print(menu.getItem(i).getName());
            if (menu.getItem(i).hasSubMenu() || menu.getItem(i).isBackItem()) {
                _u8g2.print(" >");
            }
            y += 11;
        }

        // Petits indicateurs de défilement s'il y a des items cachés
        if (start > 0) {
            _u8g2.setCursor(_u8g2.getWidth() - 6, 12);
            _u8g2.print("^");
        }
        if (start + VISIBLE_ITEMS < total) {
            _u8g2.setCursor(_u8g2.getWidth() - 6, 56);
            _u8g2.print("v");
        }
    } while (_u8g2.nextPage());
}

void DisplayManager::displayMessage(const char* msg) {
    _u8g2.firstPage();
    do {
        _u8g2.setCursor(0, 0);
        _u8g2.print(msg);
    } while (_u8g2.nextPage());
}

void DisplayManager::displayMessage(const char* line1, const char* line2) {
    _u8g2.firstPage();
    do {
        _u8g2.setCursor(0, 0);
        _u8g2.print(line1);
        if (line2) {
            _u8g2.setCursor(0, 12);
            _u8g2.print(line2);
        }
    } while (_u8g2.nextPage());
}

// Dessine un petit bonhomme allumette dans l'une des deux poses de course.
void DisplayManager::drawStickFigure(int x, int y, bool poseA) {
    // Tête
    _u8g2.drawDisc(x, y - 14, 3);
    // Corps
    _u8g2.drawLine(x, y - 11, x, y - 2);

    // Bras (alternent selon la pose -> effet de balancement)
    if (poseA) {
        _u8g2.drawLine(x, y - 9, x - 5, y - 5);
        _u8g2.drawLine(x, y - 9, x + 5, y - 12);
    } else {
        _u8g2.drawLine(x, y - 9, x - 5, y - 12);
        _u8g2.drawLine(x, y - 9, x + 5, y - 5);
    }

    // Jambes (alternent en phase opposée aux bras -> foulée de course)
    if (poseA) {
        _u8g2.drawLine(x, y - 2, x - 6, y + 6);
        _u8g2.drawLine(x, y - 2, x + 4, y + 2);
    } else {
        _u8g2.drawLine(x, y - 2, x - 4, y + 2);
        _u8g2.drawLine(x, y - 2, x + 6, y + 6);
    }
}

void DisplayManager::playStartupAnimation() {
    const int totalFrames = 40;      // plus de frames pour un mouvement plus fluide
    const int startX = -10;
    const int endX = _u8g2.getWidth() + 10;

    for (int f = 0; f < totalFrames; f++) {
        int x = startX + (long)(endX - startX) * f / (totalFrames - 1);
        bool poseA = ((f / 2) % 2 == 0); // change de pose tous les 2 frames, pas à chaque frame

        _u8g2.firstPage();
        do {
            _u8g2.setCursor(0, 0);
            _u8g2.print("Demarrage...");
            drawStickFigure(x, 45, poseA);
        } while (_u8g2.nextPage());

        delay(30); // en mode full buffer, 30ms suffit largement (~33 fps)
    }
}

void DisplayManager::playIdleAnimation(const std::function<bool()>& shouldAbort) {
    // Nécessite randomSeed() appelé une fois dans setup() (ex: randomSeed(esp_random());)
    switch (random(6)) {
        case 0:  idleAnimSleep(shouldAbort);      break;
        case 1:  idleAnimBounce(shouldAbort);     break;
        case 2:  idleAnimWave(shouldAbort);       break;
        case 3:  idleAnimLogoBounce(shouldAbort); break;
        default: idleAnimClock(shouldAbort);      break;   // cas 4 et 5 : l'horloge revient plus souvent
    }
}

void DisplayManager::playLogoSplash() {
    int x = (_u8g2.getWidth() - LOGO_SPLASH_W) / 2;
    int y = 2;

    _u8g2.firstPage();
    do {
        _u8g2.drawXBMP(x, y, LOGO_SPLASH_W, LOGO_SPLASH_H, logo_splash_bits);

        // "KAIROSLAB" en police système (le texte du logo original n'est
        // pas lisible en bitmap à cette résolution, cf. discussion projet)
        _u8g2.setFont(u8g2_font_6x10_tf);
        const char* label = "KAIROSLAB";
        int textWidth = _u8g2.getStrWidth(label);
        _u8g2.setCursor((_u8g2.getWidth() - textWidth) / 2, y + LOGO_SPLASH_H + 4);
        _u8g2.print(label);
    } while (_u8g2.nextPage());

    delay(1500);
}

void DisplayManager::idleAnimSleep(const std::function<bool()>& shouldAbort) {
    const int totalFrames = 20;
    int cx = _u8g2.getWidth() / 2;
    int cy = 36;

    for (int f = 0; f < totalFrames; f++) {
        if (shouldAbort()) return;

        _u8g2.firstPage();
        do {
            // Visage endormi
            _u8g2.drawCircle(cx, cy, 14);
            _u8g2.drawLine(cx - 7, cy - 2, cx - 3, cy - 2); // oeil gauche fermé
            _u8g2.drawLine(cx + 3, cy - 2, cx + 7, cy - 2); // oeil droit fermé
            _u8g2.drawLine(cx - 4, cy + 6, cx + 4, cy + 6); // petit sourire

            // "z" qui s'envolent en boucle vers le haut-droit
            for (int z = 0; z < 3; z++) {
                int offset = (f * 3 + z * 14) % 42;
                _u8g2.setCursor(cx + 10 + z * 4, cy - 10 - offset);
                _u8g2.print("z");
            }
        } while (_u8g2.nextPage());
        delay(90);
    }
}

void DisplayManager::idleAnimBounce(const std::function<bool()>& shouldAbort) {
    const int totalFrames = 70;
    int w = _u8g2.getWidth();
    int h = _u8g2.getHeight();
    int x = 6, y = 20;
    int vx = 2, vy = 1; // vitesse réduite, compensée par un framerate plus élevé -> plus fluide
    const int r = 4;

    for (int f = 0; f < totalFrames; f++) {
        if (shouldAbort()) return;

        x += vx;
        y += vy;
        if (x - r <= 0 || x + r >= w) vx = -vx;
        if (y - r <= 12 || y + r >= h) vy = -vy; // 12px pour laisser un peu d'air en haut

        _u8g2.firstPage();
        do {
            _u8g2.drawDisc(x, y, r);
        } while (_u8g2.nextPage());
        delay(25); // ~40fps, mouvement bien plus fluide qu'à 50ms/2px
    }
}

void DisplayManager::idleAnimWave(const std::function<bool()>& shouldAbort) {
    const int totalFrames = 16;
    int x = _u8g2.getWidth() / 2;
    int y = 45;

    for (int f = 0; f < totalFrames; f++) {
        if (shouldAbort()) return;

        bool armUp = (f % 2 == 0);

        _u8g2.firstPage();
        do {
            _u8g2.drawDisc(x, y - 14, 3);              // tête
            _u8g2.drawLine(x, y - 11, x, y - 2);        // corps
            _u8g2.drawLine(x, y - 9, x - 4, y - 3);     // bras gauche, statique

            // bras droit qui salue
            if (armUp) {
                _u8g2.drawLine(x, y - 9, x + 6, y - 16);
            } else {
                _u8g2.drawLine(x, y - 9, x + 6, y - 9);
            }

            _u8g2.drawLine(x, y - 2, x - 4, y + 6);     // jambe gauche
            _u8g2.drawLine(x, y - 2, x + 4, y + 6);     // jambe droite
        } while (_u8g2.nextPage());
        delay(120);
    }
}

void DisplayManager::idleAnimLogoBounce(const std::function<bool()>& shouldAbort) {
    const int totalFrames = 90;
    int w = _u8g2.getWidth();
    int h = _u8g2.getHeight();
    int x = 4, y = 4;
    int vx = 2, vy = 2;

    for (int f = 0; f < totalFrames; f++) {
        if (shouldAbort()) return;

        x += vx;
        y += vy;
        if (x <= 0 || x + LOGO_BOUNCE_W >= w) vx = -vx;
        if (y <= 0 || y + LOGO_BOUNCE_H >= h) vy = -vy;

        _u8g2.firstPage();
        do {
            _u8g2.drawXBMP(x, y, LOGO_BOUNCE_W, LOGO_BOUNCE_H, logo_bounce_bits);
        } while (_u8g2.nextPage());
        delay(30);
    }
}

// Horloge de veille : cadran analogique à gauche, heure + date à droite.
void DisplayManager::idleAnimClock(const std::function<bool()>& shouldAbort) {
    const int cx = 31, cy = 32, r = 29;
    const unsigned long durationMs = 12000;
    const unsigned long start = millis();

    auto px = [&](float len, float a) { return (int)lroundf(cx + len * sinf(a)); };
    auto py = [&](float len, float a) { return (int)lroundf(cy - len * cosf(a)); };

    while (millis() - start < durationMs) {
        if (shouldAbort()) return;

        ClockInfo c = _clock ? _clock() : ClockInfo();

        _u8g2.firstPage();
        do {
            // Cadran + graduations (plus longues toutes les 3 heures)
            _u8g2.drawCircle(cx, cy, r);
            for (int i = 0; i < 12; i++) {
                float a = i * (float)M_PI / 6.0f;
                float len = (i % 3 == 0) ? 6 : 3;
                _u8g2.drawLine(px(r - len, a), py(r - len, a), px(r - 1, a), py(r - 1, a));
            }

            if (c.valid) {
                float aS = c.second * (float)M_PI / 30.0f;
                float aM = (c.minute + c.second / 60.0f) * (float)M_PI / 30.0f;
                float aH = ((c.hour % 12) + c.minute / 60.0f) * (float)M_PI / 6.0f;
                // aiguilles heure/minute épaisses (2 traits parallèles)
                _u8g2.drawLine(cx, cy, px(14, aH), py(14, aH));
                _u8g2.drawLine(cx + 1, cy, px(14, aH) + 1, py(14, aH));
                _u8g2.drawLine(cx, cy, px(21, aM), py(21, aM));
                _u8g2.drawLine(cx, cy + 1, px(21, aM), py(21, aM) + 1);
                // trotteuse fine
                _u8g2.drawLine(cx, cy, px(25, aS), py(25, aS));
            }
            _u8g2.drawDisc(cx, cy, 2);

            // Heure numérique, deux-points clignotants
            char buf[12];
            if (c.valid) snprintf(buf, sizeof(buf), (c.second % 2) ? "%02d %02d" : "%02d:%02d", c.hour, c.minute);
            else         snprintf(buf, sizeof(buf), "--:--");
            _u8g2.setFont(u8g2_font_helvB14_tr);
            _u8g2.setCursor(68, 6);
            _u8g2.print(buf);

            // Date + jour de la semaine
            _u8g2.setFont(u8g2_font_6x10_tf);
            if (c.valid) {
                snprintf(buf, sizeof(buf), "%02d/%02d/%04d", c.day, c.month, c.year);
                _u8g2.setCursor(66, 30);
                _u8g2.print(buf);
                _u8g2.setCursor(66, 42);
                _u8g2.print(I18n::dayShort(c.dow));
            } else {
                _u8g2.setCursor(66, 30);
                _u8g2.print(I18n::tr("Heure non", "Time not"));
                _u8g2.setCursor(66, 42);
                _u8g2.print(I18n::tr("reglee", "set"));
            }
        } while (_u8g2.nextPage());

        delay(100);
    }
}
