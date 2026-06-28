#pragma once

#include <Preferences.h>
#include <string.h>
#include "config.h"

#define MAX_SENSORS 50
#define MAC_STR_LEN 18   // "AA:BB:CC:DD:EE:FF" + '\0'

// In-Memory-Abbild der aktiven Sensorliste
static char   sensorMacs[MAX_SENSORS][MAC_STR_LEN];
static size_t sensorCount = 0;

static Preferences prefs;

// Persistiert die aktuelle Sensorliste in den NVS-Namespace "flora".
void saveSensors() {
  prefs.begin("flora", false);
  prefs.putUInt("count", (uint32_t)sensorCount);
  for (size_t i = 0; i < sensorCount; i++) {
    char key[8];
    snprintf(key, sizeof(key), "mac%u", (unsigned)i);
    prefs.putString(key, sensorMacs[i]);
  }
  prefs.end();
}

// Laedt die persistierte Sensorliste aus dem NVS. Beim Erststart (NVS leer)
// werden die Defaults aus config.h uebernommen und gespeichert.
void loadSensors() {
  prefs.begin("flora", true);  // read-only
  sensorCount = prefs.getUInt("count", 0);
  if (sensorCount > MAX_SENSORS) sensorCount = MAX_SENSORS;
  for (size_t i = 0; i < sensorCount; i++) {
    char key[8];
    snprintf(key, sizeof(key), "mac%u", (unsigned)i);
    String mac = prefs.getString(key, "");
    strncpy(sensorMacs[i], mac.c_str(), MAC_STR_LEN - 1);
    sensorMacs[i][MAC_STR_LEN - 1] = '\0';
  }
  prefs.end();

  if (sensorCount == 0) {
    size_t defaults = sizeof(FLORA_DEVICES) / sizeof(FLORA_DEVICES[0]);
    for (size_t i = 0; i < defaults && i < MAX_SENSORS; i++) {
      strncpy(sensorMacs[i], FLORA_DEVICES[i], MAC_STR_LEN - 1);
      sensorMacs[i][MAC_STR_LEN - 1] = '\0';
      sensorCount++;
    }
    Serial.printf("Sensor store empty, seeded %u sensor(s) from config\n",
                  (unsigned)sensorCount);
    saveSensors();
  } else {
    Serial.printf("Loaded %u sensor(s) from store\n", (unsigned)sensorCount);
  }
}

// Prueft, ob eine MAC (case-insensitive) bereits bekannt ist.
bool sensorKnown(const char* mac) {
  for (size_t i = 0; i < sensorCount; i++) {
    if (strcasecmp(sensorMacs[i], mac) == 0) return true;
  }
  return false;
}

// Fuegt eine neue MAC hinzu. Gibt true zurueck, wenn es ein echter Neuzugang war.
bool addSensor(const char* mac) {
  if (sensorCount >= MAX_SENSORS) return false;
  if (sensorKnown(mac)) return false;
  strncpy(sensorMacs[sensorCount], mac, MAC_STR_LEN - 1);
  sensorMacs[sensorCount][MAC_STR_LEN - 1] = '\0';
  sensorCount++;
  Serial.printf("Added sensor: %s (now %u total)\n", mac, (unsigned)sensorCount);
  return true;
}
