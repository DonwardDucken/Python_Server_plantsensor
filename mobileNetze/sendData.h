#include <ArduinoHttpClient.h>
#pragma once
//#include <HTTPClient.h>
#include <WiFi.h>
#include "config.h"
#include <cJSON.h>

#define MAX_SENSORS 50
size_t mac_count =0;
char macs[MAX_SENSORS][18];

void parse_response(const char* json_text)
{
    cJSON *root = cJSON_Parse(json_text);
    if (!root) return;

    cJSON *sensors = cJSON_GetObjectItem(root, "new_sensors");

    if (cJSON_IsArray(sensors))
    {
        mac_count = cJSON_GetArraySize(sensors);

        for (int i = 0; i < mac_count && i < MAX_SENSORS; i++)
        {
            cJSON *sensor = cJSON_GetArrayItem(sensors, i);
            cJSON *mac = cJSON_GetObjectItem(sensor, "mac");

            if (cJSON_IsString(mac))
            {
                strncpy(macs[i], mac->valuestring, sizeof(macs[i]) - 1);
                macs[i][17] = '\0';
            }
        }
    }

    cJSON_Delete(root);
}

void saveMac(char mac[][18], size_t length){
  for (int i =0; i<length;i++) {
    FLORA_DEVICES[i] = mac[i];
    Serial.printf("%s", mac[i]);
  }
}

void postToServer(float temperature, int light, int moisture, int conductivity, char* mac, WiFiClient wifi) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skip POST");
    return;
  }
  HttpClient http = HttpClient(wifi, SERVER_ADDRESS, SERVER_PORT);
  //http.begin(SERVER_URL);
  //http.addHeader("Content-Type", "text/plain");
  char body[256];
  snprintf(body, 256,
           "{\"mac\":\"%s\",\"temp\":%.1f,\"light\":%d,\"moisture\":%d,\"conductivity\":%d}", mac, temperature, light, moisture, conductivity);
  Serial.println("POST to server...");
  Serial.println(body);

  int httpCode = http.post("/", "text/plain", body);
  int statusCode  = http.responseStatusCode();
  String response = http.responseBody();
  Serial.printf(" %d", response.length());
  Serial.printf("Status Code: %d ", statusCode);
  Serial.printf("Response: %s",response);
  parse_response(response.c_str());
  saveMac(macs,mac_count);
  //http.end();
}

