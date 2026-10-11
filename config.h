#ifndef CONFIG_H
#define CONFIG_H

// ---------------- Matériel ----------------
// Pins I2C (OLED + DS3231 sur le même bus).
// ATTENTION : sur les ESP32-C3 SuperMini la LED bleue est sur GPIO8 :
// on utilise donc 6/7, libres de tout conflit.
#define I2C_SDA 6
#define I2C_SCL 7

// Rotary encoder
#define ENC_CLK  2
#define ENC_DT   3
#define ENC_SW   4

// Relais de sonnerie (GPIO5 : libre, ni strapping ni LED)
#define PIN_RELAY 5
// Modules relais "opto" bon marché : déclenchés au niveau BAS.
// Mettre false si le tien s'active au niveau HAUT.
#define RELAY_ACTIVE_LOW true

// ---------------- Sonnerie ----------------
// Motif automatique (horaires programmés) : plusieurs impulsions courtes
#define BELL_PULSE_COUNT   3
#define BELL_PULSE_ON_MS   400
#define BELL_PULSE_OFF_MS  300

// Sonnerie "maintenue" (écran OLED ou bouton web) :
// le relais reste fermé tant que le bouton est maintenu...
#define BELL_HOLD_MAX_MS      60000  // ...mais jamais plus de 60 s (sécurité)
#define BELL_HOLD_WATCHDOG_MS 1500   // coupe si la page web n'envoie plus de "heartbeat"
#define RING_SCREEN_TIMEOUT_MS 5000  // retour au menu après 5 s sans appui

// ---------------- Affichage ----------------
#define DISPLAY_REFRESH_MS 30
#define ENCODER_DEBOUNCE_MS 5
#define IDLE_TIMEOUT_MS 120000  // 2 minutes avant les animations de veille

// ---------------- Réseau / heure ----------------
#define AP_NAME            "KairosLab-Setup"   // hotspot de configuration Wi-Fi
#define HOSTNAME           "kairoslab"         // http://kairoslab.local
#define PORTAL_TIMEOUT_S   180
#define WIFI_RECONNECT_WAIT_MS 10000
#define NTP_RESYNC_MS      (6UL * 3600UL * 1000UL)
#define TZ_POSIX           "WAT-1"             // Cameroun : UTC+1, pas d'heure d'été

#endif
