#include "Screens.h"
#include "config.h"
#include "I18n.h"
#include <WiFi.h>
#include <math.h>

// ------------------------------------------------------------------
// Utilitaires de dessin
// ------------------------------------------------------------------
static void text(U8G2& u, int x, int y, const char* s) {
    u.setCursor(x, y);
    u.print(s);
}

static void textCentered(U8G2& u, int y, const char* s) {
    int w = u.getUTF8Width(s);
    text(u, (u.getWidth() - w) / 2, y, s);
}

// Raccourcit une chaîne UTF-8 pour qu'elle tienne dans maxW pixels
static void fitText(U8G2& u, const char* src, char* out, size_t cap, int maxW) {
    strncpy(out, src, cap - 1);
    out[cap - 1] = 0;
    while (out[0] && u.getUTF8Width(out) > maxW) {
        size_t n = strlen(out) - 1;
        while (n > 0 && (out[n] & 0xC0) == 0x80) n--;   // ne coupe pas au milieu d'un caractère
        out[n] = 0;
    }
}

// Symbole "power" : arc ouvert en haut + barre verticale
static void drawPowerSymbol(U8G2& u, int cx, int cy, int r) {
    for (int rr = r - 2; rr <= r; rr++) {
        int px = 0, py = 0;
        bool first = true;
        for (int deg = 40; deg <= 320; deg += 8) {
            float a = deg * (float)M_PI / 180.0f;
            int x = cx + (int)lroundf(rr * sinf(a));
            int y = cy - (int)lroundf(rr * cosf(a));
            if (!first) u.drawLine(px, py, x, y);
            px = x; py = y; first = false;
        }
    }
    for (int dx = -1; dx <= 1; dx++) u.drawLine(cx + dx, cy - r - 2, cx + dx, cy - 1);
}

static int daysInMonth(int y, int m) {
    static const uint8_t d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m - 1];
}

// ------------------------------------------------------------------
Screens::Screens(DisplayManager& display, Encoder& encoder, RelayController& relay,
                 RtcManager& rtc, NetworkManager& net, bool& timeTrusted,
                 std::function<void()> backgroundTick)
    : _d(display), _enc(encoder), _relay(relay), _rtc(rtc), _net(net),
      _trusted(timeTrusted), _tick(backgroundTick) {}

void Screens::pump() {
    _enc.update();
    if (_tick) _tick();
}

// Attend que le bouton soit relâché : l'appui qui a ouvert l'écran ne doit pas
// être pris pour une action de l'écran lui-même (surtout pour le relais !).
void Screens::waitRelease() {
    unsigned long t0 = millis();
    do { pump(); delay(5); } while (_enc.isButtonHeld() && millis() - t0 < 4000);
    _enc.clearButtonEvent();
    _enc.resetCounter();
}

void Screens::toast(const char* l1, const char* l2, uint16_t ms) {
    _d.displayMessage(l1, l2);
    unsigned long s = millis();
    while (millis() - s < ms) { pump(); delay(10); }
    _enc.clearButtonEvent();
    _enc.resetCounter();
}

// ------------------------------------------------------------------
// Sonner maintenant : symbole power au centre.
//  - 5 s sans appui sur le bouton -> retour au menu précédent
//  - appui maintenu -> le relais reste fermé jusqu'au relâchement
// ------------------------------------------------------------------
void Screens::ringNow() {
    waitRelease();
    U8G2& u = _d.raw();

    unsigned long idleStart = millis();
    bool wasHeld = false, latchedOff = false, lastShown = false;
    int lastBar = -1;
    bool needDraw = true;

    while (true) {
        pump();
        bool held = _enc.isButtonHeld();

        if (held) {
            idleStart = millis();
            // Si la sécurité du relais (durée max) a coupé, on exige un relâchement avant de réarmer
            if (wasHeld && !_relay.isHeld()) latchedOff = true;
            if (!latchedOff) { _relay.holdOn(); _relay.feedHold(); }
        } else {
            if (wasHeld) { _relay.holdOff(); idleStart = millis(); }
            latchedOff = false;
        }
        wasHeld = held;

        unsigned long elapsed = millis() - idleStart;
        if (!held && elapsed >= RING_SCREEN_TIMEOUT_MS) break;

        bool on = held && _relay.isHeld();
        int bar = held ? 100 : (int)(100UL * (RING_SCREEN_TIMEOUT_MS - elapsed) / RING_SCREEN_TIMEOUT_MS);
        if (on != lastShown || bar != lastBar || needDraw) {
            lastShown = on; lastBar = bar; needDraw = false;
            u.firstPage();
            do {
                u.setFont(u8g2_font_6x10_tf);
                textCentered(u, 0, I18n::tr("Sonner maintenant", "Ring now"));
                if (on) {
                    u.drawDisc(64, 31, 17);          // bouton "enfoncé" : disque plein
                    u.setDrawColor(0);
                    drawPowerSymbol(u, 64, 31, 12);
                    u.setDrawColor(1);
                    textCentered(u, 51, I18n::tr("SONNE !", "RINGING!"));
                } else {
                    drawPowerSymbol(u, 64, 31, 13);
                    textCentered(u, 51, I18n::tr("Maintenir = sonner", "Hold = ring"));
                    u.drawFrame(14, 62, 100, 2);     // compte à rebours avant retour au menu
                    u.drawBox(14, 62, bar, 2);
                }
            } while (u.nextPage());
        }
        delay(5);
    }

    _relay.holdOff();
    _enc.clearButtonEvent();
    _enc.resetCounter();
}

// ------------------------------------------------------------------
// Statut / heure
// ------------------------------------------------------------------
void Screens::status() {
    waitRelease();
    U8G2& u = _d.raw();
    unsigned long start = millis(), lastDraw = 0;

    while (millis() - start < 20000) {
        pump();
        if (_enc.isButtonPressed()) break;

        if (millis() - lastDraw >= 500) {
            lastDraw = millis();
            DateTime n = _rtc.now();
            char l1[28], l2[28], l3[28], l4[28];
            snprintf(l1, sizeof(l1), "%02d/%02d/%04d %02d:%02d:%02d", n.day(), n.month(), n.year(), n.hour(), n.minute(), n.second());
            snprintf(l2, sizeof(l2), "%s", _trusted ? I18n::tr("Heure fiable", "Time reliable") : I18n::tr("Heure NON fiable !", "Time NOT reliable!"));
            snprintf(l3, sizeof(l3), "%s %s", I18n::tr("Source :", "Source:"), _rtc.hasHardware() ? "DS3231" : "ESP32");
            if (_net.isConnected()) snprintf(l4, sizeof(l4), "Wi-Fi %s", _net.ip().c_str());
            else                    snprintf(l4, sizeof(l4), "Wi-Fi %s", I18n::tr("hors ligne", "offline"));

            u.firstPage();
            do {
                u.setFont(u8g2_font_6x10_tf);
                text(u, 0, 0, l1);
                text(u, 0, 14, l2);
                text(u, 0, 26, l3);
                text(u, 0, 38, l4);
                text(u, 0, 53, I18n::tr("Clic = retour", "Click = back"));
            } while (u.nextPage());
        }
        delay(10);
    }
    _enc.clearButtonEvent();
    _enc.resetCounter();
}

// ------------------------------------------------------------------
// Wi-Fi : connecté -> adresse de l'interface web ; sinon -> hotspot
// ------------------------------------------------------------------
void Screens::wifi() {
    waitRelease();
    U8G2& u = _d.raw();

    // Un réseau est mémorisé mais pas encore connecté : on lui laisse un peu de temps
    if (!_net.isConnected() && _net.hasSavedNetwork()) {
        unsigned long start = millis();
        while (!_net.isConnected() && millis() - start < WIFI_RECONNECT_WAIT_MS) {
            pump();
            if (_enc.isButtonPressed()) { _enc.clearButtonEvent(); return; }   // annulation
            char dots[8];
            snprintf(dots, sizeof(dots), "%.*s", (int)((millis() / 400) % 4), "...");
            u.firstPage();
            do {
                u.setFont(u8g2_font_6x10_tf);
                text(u, 0, 8, I18n::tr("Connexion Wi-Fi", "Connecting Wi-Fi"));
                text(u, 0, 22, dots);
                text(u, 0, 52, I18n::tr("Clic = annuler", "Click = cancel"));
            } while (u.nextPage());
            delay(50);
        }
    }

    if (_net.isConnected()) {
        if (wifiInfo()) portalFlow();   // appui long : changer de réseau
    } else {
        portalFlow();
    }
    _enc.clearButtonEvent();
    _enc.resetCounter();
}

bool Screens::wifiInfo() {
    U8G2& u = _d.raw();
    unsigned long start = millis(), pressStart = 0, lastDraw = 0;

    while (millis() - start < 25000) {
        pump();

        if (_enc.isButtonHeld()) {
            if (!pressStart) pressStart = millis();
            if (millis() - pressStart >= 1200) { waitRelease(); return true; }   // appui long
        } else if (pressStart) {
            _enc.clearButtonEvent();
            return false;                                                         // appui court
        }

        if (millis() - lastDraw >= 400) {
            lastDraw = millis();
            char ssid[32], url[32];
            fitText(u, _net.ssid().c_str(), ssid, sizeof(ssid), 124);
            snprintf(url, sizeof(url), "http://%s", _net.ip().c_str());   // 19 caractères max : tient sur 128 px

            u.firstPage();
            do {
                u.setFont(u8g2_font_6x10_tf);
                text(u, 0, 0, I18n::tr("Wi-Fi connecté", "Wi-Fi connected"));
                u.drawHLine(0, 11, 128);
                text(u, 0, 14, ssid);
                text(u, 0, 28, I18n::tr("Interface web :", "Web interface:"));
                text(u, 0, 40, url);
                text(u, 0, 54, I18n::tr("Maintenir: autre WiFi", "Hold: change network"));
            } while (u.nextPage());
        }
        delay(10);
    }
    return false;
}

void Screens::portalFlow() {
    U8G2& u = _d.raw();

    if (!_net.startPortal()) {
        _net.stopPortal();
        toast(I18n::tr("Hotspot: echec", "Hotspot failed"));
        return;
    }

    bool connected = false;
    unsigned long lastDraw = 0;

    while (true) {
        pump();
        _net.processPortal();

        if (_net.portalElapsedMs() > 1500 && WiFi.status() == WL_CONNECTED) { connected = true; break; }
        if (!_net.portalActive()) break;                                   // délai expiré
        if (_enc.isButtonPressed()) { _enc.clearButtonEvent(); break; }    // annulé

        if (millis() - lastDraw >= 250) {
            lastDraw = millis();
            int remaining = PORTAL_TIMEOUT_S - (int)(_net.portalElapsedMs() / 1000);
            if (remaining < 0) remaining = 0;
            char foot[24];
            snprintf(foot, sizeof(foot), "%s %ds", I18n::tr("Clic=annuler", "Click=cancel"), remaining);

            u.firstPage();
            do {
                u.setFont(u8g2_font_6x10_tf);
                text(u, 0, 0, I18n::tr("Rejoins le Wi-Fi :", "Join the Wi-Fi:"));
                text(u, 0, 11, AP_NAME);
                text(u, 0, 24, I18n::tr("puis ouvre :", "then open:"));
                text(u, 0, 35, "192.168.4.1");
                text(u, 0, 52, foot);
            } while (u.nextPage());
        }
        delay(10);
    }

    if (connected) {
        // Laisse au navigateur le temps d'afficher la page "enregistré"
        unsigned long t = millis();
        while (millis() - t < 1500) { pump(); _net.processPortal(); delay(10); }
    }
    _net.stopPortal();

    if (connected) {
        toast(I18n::tr("Wi-Fi enregistré", "Wi-Fi saved"), I18n::tr("Connexion OK", "Connected"), 1200);
        wifiInfo();
    } else {
        toast(I18n::tr("Configuration", "Setup"), I18n::tr("annulée", "cancelled"), 1200);
    }
}

// ------------------------------------------------------------------
// Rappel de l'adresse de l'interface web (entrées Horaires / Vacances)
// ------------------------------------------------------------------
void Screens::webInfo() {
    waitRelease();
    U8G2& u = _d.raw();
    bool conn = _net.isConnected();
    char url[32];
    snprintf(url, sizeof(url), "http://%s", _net.ip().c_str());

    u.firstPage();
    do {
        u.setFont(u8g2_font_6x10_tf);
        text(u, 0, 0, I18n::tr("Gestion via le web", "Managed via web"));
        u.drawHLine(0, 11, 128);
        if (conn) {
            text(u, 0, 18, I18n::tr("Ouvre dans ton", "Open in your"));
            text(u, 0, 30, I18n::tr("navigateur :", "browser:"));
            text(u, 0, 44, url);
        } else {
            text(u, 0, 18, I18n::tr("Wi-Fi requis :", "Wi-Fi needed:"));
            text(u, 0, 32, I18n::tr("Connexion > Wi-Fi", "Connection > Wi-Fi"));
        }
    } while (u.nextPage());

    unsigned long s = millis();
    while (millis() - s < 6000) {
        pump();
        if (_enc.isButtonPressed()) break;
        delay(10);
    }
    _enc.clearButtonEvent();
    _enc.resetCounter();
}

// ------------------------------------------------------------------
// Réglage > Heure/date : rotation = changer la valeur, clic = champ suivant,
// puis confirmation Valider / Annuler.
// ------------------------------------------------------------------
void Screens::dateTimeEditor() {
    waitRelease();
    U8G2& u = _d.raw();

    int f[5];   // jour, mois, année, heure, minute
    if (_trusted) {
        DateTime n = _rtc.now();
        f[0] = n.day(); f[1] = n.month(); f[2] = n.year(); f[3] = n.hour(); f[4] = n.minute();
    } else {
        f[0] = 1; f[1] = 1; f[2] = 2026; f[3] = 8; f[4] = 0;
    }

    int stage = 0;               // 0..4 = champs, 5 = confirmation
    bool save = true, dirty = true;
    unsigned long last = millis();

    auto wrap = [](int v, int lo, int hi, int d) {
        int range = hi - lo + 1;
        return lo + (((v - lo + d) % range) + range) % range;
    };

    while (true) {
        pump();

        int delta = _enc.getCounter();
        if (delta != 0) {
            _enc.resetCounter();
            last = millis();
            dirty = true;
            switch (stage) {
                case 0: f[0] = wrap(f[0], 1, daysInMonth(f[2], f[1]), delta); break;
                case 1: f[1] = wrap(f[1], 1, 12, delta); break;
                case 2: f[2] = wrap(f[2], 2024, 2099, delta); break;
                case 3: f[3] = wrap(f[3], 0, 23, delta); break;
                case 4: f[4] = wrap(f[4], 0, 59, delta); break;
                default: save = !save; break;
            }
            int dim = daysInMonth(f[2], f[1]);
            if (f[0] > dim) f[0] = dim;     // ex: 31 -> 28 si on passe à février
        }

        if (_enc.isButtonPressed()) {
            _enc.clearButtonEvent();
            last = millis();
            dirty = true;
            if (stage < 5) {
                stage++;
            } else {
                if (save) {
                    _rtc.adjust(DateTime(f[2], f[1], f[0], f[3], f[4], 0));
                    _trusted = true;
                    toast(I18n::tr("Heure enregistrée", "Time saved"));
                }
                return;
            }
        }

        if (millis() - last > 60000) return;   // inactivité : on quitte sans enregistrer

        if (dirty) {
            dirty = false;
            char d0[4], d1[4], d2[6], t0[4], t1[4];
            snprintf(d0, sizeof(d0), "%02d", f[0]);
            snprintf(d1, sizeof(d1), "%02d", f[1]);
            snprintf(d2, sizeof(d2), "%04d", f[2]);
            snprintf(t0, sizeof(t0), "%02d", f[3]);
            snprintf(t1, sizeof(t1), "%02d", f[4]);

            u.firstPage();
            do {
                u.setFont(u8g2_font_6x10_tf);
                text(u, 0, 0, I18n::tr("Heure / date", "Time / date"));
                u.drawHLine(0, 11, 128);
                u.setFont(u8g2_font_8x13_tf);

                // Champ en surbrillance (vidéo inversée)
                auto field = [&](int x, int y, const char* s, bool active) {
                    int w = u.getStrWidth(s);
                    if (active) {
                        u.setDrawColor(1);
                        u.drawBox(x - 1, y - 1, w + 2, 14);
                        u.setDrawColor(0);
                    }
                    text(u, x, y, s);
                    u.setDrawColor(1);
                };

                field(24, 17, d0, stage == 0);  text(u, 40, 17, "/");
                field(48, 17, d1, stage == 1);  text(u, 64, 17, "/");
                field(72, 17, d2, stage == 2);
                field(44, 35, t0, stage == 3);  text(u, 60, 35, ":");
                field(68, 35, t1, stage == 4);

                u.setFont(u8g2_font_6x10_tf);
                if (stage < 5) {
                    text(u, 0, 54, I18n::tr("Tourner / Clic=suivant", "Turn / Click=next"));
                } else {
                    u.drawBox(0, 50, 128, 14);
                    u.setDrawColor(0);
                    text(u, 4, 53, save ? I18n::tr("> Valider    Annuler", "> Save       Cancel")
                                        : I18n::tr("  Valider  > Annuler", "  Save     > Cancel"));
                    u.setDrawColor(1);
                }
            } while (u.nextPage());
        }
        delay(5);
    }
}
