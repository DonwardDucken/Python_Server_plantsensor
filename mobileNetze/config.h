#pragma once

// ─── Xiaomi Flora Sensoren ────────────────────────────────────────────
// Initiale Sensoren — beim Erststart in den NVS-Store (sensorStore.h)
// uebernommen. Danach ist der NVS-Store die Quelle der Wahrheit; neue
// Sensoren meldet der Server via "new_sensors".
const char* FLORA_DEVICES[] = {
    "5C:85:7E:14:48:E9",
};

// ─── Timing ───────────────────────────────────────────────────────────
#define SLEEP_DURATION      30         // Deep Sleep zwischen zwei Laeufen (Sekunden)
#define EMERGENCY_HIBERNATE (3 * 60)   // Watchdog-Notabschaltung (Sekunden)
#define RETRY               3          // Versuche pro Sensor pro Lauf

// ─── Netzwerk ─────────────────────────────────────────────────────────
const char* WIFI_SSID      = "Desktop";
const char* WIFI_PASSWORD  = "12345678";
const char* SERVER_ADDRESS = "192.168.137.1";
const int   SERVER_PORT    = 8080;
