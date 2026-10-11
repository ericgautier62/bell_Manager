/*
  KAIROSLAB - Module de sonnerie scolaire (ESP32-C3 + OLED SH1106 + encodeur + relais + DS3231)

  Bibliothèques à installer (Gestionnaire de bibliothèques) :
    - U8g2            (olikraus)
    - RTClib          (Adafruit)  [+ Adafruit BusIO si demandé]
    - WiFiManager     (tzapu)
  Intégrées à l'ESP32 (rien à installer) : WebServer, ESPmDNS, Preferences.
*/

#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include "config.h"
#include "I18n.h"
#include "Encoder.h"
#include "Menu.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "RelayController.h"
#include "RtcManager.h"
#include "ExceptionCalendar.h"
#include "ExtraDays.h"
#include "ScheduleManager.h"
#include "WebDashboard.h"
#include "NetworkManager.h"
#include "Screens.h"

// Mode "F" (full frame buffer) : animations fluides, ~1 Ko de RAM.
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

Encoder encoder(ENC_CLK, ENC_DT, ENC_SW);
DisplayManager display(u8g2);
MenuManager menuManager(display, encoder);

// --- Sous-système sonnerie ---
RelayController relay(PIN_RELAY, RELAY_ACTIVE_LOW);
RtcManager rtc;
ExceptionCalendar holidays;   // jours SANS sonnerie (fériés, vacances)
ExtraDays extras;             // jours EXCEPTIONNELS avec sonnerie (ex: samedi de rattrapage)
ScheduleManager schedules;

// Tant que l'heure n'est pas fiable (DS3231 ayant perdu l'alimentation et pas
// encore resynchronisé), les sonneries automatiques sont suspendues :
// mieux vaut ne pas sonner que sonner à une heure aléatoire.
bool timeTrusted = false;

WebDashboard web(rtc, schedules, holidays, extras, relay, timeTrusted);
NetworkManager net(web, rtc, timeTrusted);

// Tâche de fond : appelée en continu, y compris pendant les animations de veille
// et à l'intérieur des écrans dédiés.
void backgroundService() {
    relay.update();       // coupe la sonnerie maintenue si le heartbeat s'arrête
    net.service();        // Wi-Fi, serveur web, synchro NTP
    if (timeTrusted) schedules.update(rtc, holidays, extras, relay);
}

Screens screens(display, encoder, relay, rtc, net, timeTrusted, backgroundService);

// Heure fournie à l'animation d'horloge de veille
ClockInfo clockProvider() {
    ClockInfo c;
    c.valid = timeTrusted;
    if (c.valid) {
        DateTime n = rtc.now();
        c.hour = n.hour();  c.minute = n.minute();  c.second = n.second();
        c.day = n.day();    c.month = n.month();    c.year = n.year();
        c.dow = n.dayOfTheWeek();
    }
    return c;
}

// --- Menus ---
Menu mainMenu("Menu Principal", "Main Menu");
Menu subMenuConnexion("Connexion", "Connection");
Menu subMenuSonnerie("Sonnerie", "Bell");
Menu subMenuReglage("Réglage", "Settings");
Menu subMenuLangue("Langue", "Language");

// --- Actions (fonctions simples : MenuItem n'accepte que des pointeurs de fonction) ---
void actWifi()      { screens.wifi(); }
void actBluetooth() { screens.toast(I18n::tr("Bluetooth", "Bluetooth"), I18n::tr("Bientôt disponible", "Coming soon")); }
void actRingNow()   { screens.ringNow(); }
void actStatus()    { screens.status(); }
void actWebInfo()   { screens.webInfo(); }
void actDateTime()  { screens.dateTimeEditor(); }
void actLangFr()    { I18n::set(Lang::FR); screens.toast("Langue :", "Français"); }
void actLangEn()    { I18n::set(Lang::EN); screens.toast("Language:", "English"); }

void setupMenus() {
    subMenuConnexion.addItem(MenuItem("Wi-Fi", "Wi-Fi", nullptr, actWifi));
    subMenuConnexion.addItem(MenuItem("Bluetooth", "Bluetooth", nullptr, actBluetooth));
    subMenuConnexion.addItem(MenuItem::createBackItem());

    subMenuSonnerie.addItem(MenuItem("Sonner maintenant", "Ring now", nullptr, actRingNow));
    subMenuSonnerie.addItem(MenuItem("Statut / heure", "Status / time", nullptr, actStatus));
    subMenuSonnerie.addItem(MenuItem("Horaires (web)", "Schedule (web)", nullptr, actWebInfo));
    subMenuSonnerie.addItem(MenuItem("Vacances (web)", "Holidays (web)", nullptr, actWebInfo));
    subMenuSonnerie.addItem(MenuItem::createBackItem());

    subMenuLangue.addItem(MenuItem("Français", "Français", nullptr, actLangFr));
    subMenuLangue.addItem(MenuItem("English", "English", nullptr, actLangEn));
    subMenuLangue.addItem(MenuItem::createBackItem());

    subMenuReglage.addItem(MenuItem("Heure/date", "Time/date", nullptr, actDateTime));
    subMenuReglage.addItem(MenuItem("Langue", "Language", &subMenuLangue));
    subMenuReglage.addItem(MenuItem::createBackItem());

    mainMenu.addItem(MenuItem("Connexion", "Connection", &subMenuConnexion));
    mainMenu.addItem(MenuItem("Sonnerie", "Bell", &subMenuSonnerie));
    mainMenu.addItem(MenuItem("Réglage", "Settings", &subMenuReglage));
}

// --- Setup & Loop ---
void setup() {
    Serial.begin(115200);

    // Graine du RNG matériel : choix aléatoire des animations de veille
    randomSeed(esp_random());

    // Relais au repos en tout premier : aucune impulsion parasite au démarrage
    relay.begin();

    // I2C AVANT l'écran et le RTC (même bus)
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);

    I18n::begin();            // langue enregistrée
    encoder.begin();
    display.begin();
    display.setClockProvider(clockProvider);

    // RTC DS3231 (ou repli sur l'horloge de l'ESP32 si absent)
    if (!rtc.begin()) {
        Serial.println("DS3231 non detecte : repli sur l'horloge ESP32 (synchro NTP/manuelle requise).");
    }
    timeTrusted = rtc.hasValidTime();
    if (!timeTrusted) {
        Serial.println("Heure non fiable : sonneries automatiques suspendues jusqu'au reglage.");
    }

    holidays.begin();
    extras.begin();
    schedules.begin();

    display.playLogoSplash();
    display.playStartupAnimation();

    net.begin();              // reconnexion au Wi-Fi mémorisé, en arrière-plan

    setupMenus();
    menuManager.setBackgroundTask(backgroundService);
    menuManager.pushMenu(&mainMenu);
}

void loop() {
    encoder.update();
    menuManager.update();     // appelle aussi backgroundService()
    delay(DISPLAY_REFRESH_MS);
}
