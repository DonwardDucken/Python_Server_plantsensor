#pragma once

#include <ArduinoHttpClient.h>
#include <WiFi.h>
#include <cJSON.h>
#include "config.h"
#include "sensorStore.h"

// Parst die Server-Antwort und uebernimmt neue Sensoren aus dem Array
// "new_sensors". Persistiert nur, wenn tatsaechlich ein neuer Sensor dazukam.
void parseResponse(const char* json) {
  cJSON* root = cJSON_Parse(json);
  if (!root) return;

  cJSON* sensors = cJSON_GetObjectItem(root, "new_sensors");
  if (cJSON_IsArray(sensors)) {
    bool changed = false;
    int count = cJSON_GetArraySize(sensors);
    for (int i = 0; i < count; i++) {
      cJSON* sensor = cJSON_GetArrayItem(sensors, i);
      cJSON* mac = cJSON_GetObjectItem(sensor, "mac");
      if (cJSON_IsString(mac) && mac->valuestring) {
        if (addSensor(mac->valuestring)) changed = true;
      }
    }
    if (changed) saveSensors();
  }

  cJSON_Delete(root);
}

// Sendet einen Messwert-Satz als JSON an den konfigurierten Server und
// verarbeitet dessen Antwort.
void postToServer(WiFiClient& wifi, const char* mac,
                  float temperature, int light, int moisture, int conductivity) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skip POST");
    return;
  }

  char body[256];
  snprintf(body, sizeof(body),
           "{\"mac\":\"%s\",\"temp\":%.1f,\"light\":%d,\"moisture\":%d,\"conductivity\":%d}",
           mac, temperature, light, moisture, conductivity);

  Serial.print("POST to server: ");
  Serial.println(body);

  HttpClient http(wifi, SERVER_ADDRESS, SERVER_PORT);
  http.post("/", "text/plain", body);

  int statusCode = http.responseStatusCode();
  String response = http.responseBody();
  Serial.printf("Status: %d  Response: %s\n", statusCode, response.c_str());

  parseResponse(response.c_str());
}
