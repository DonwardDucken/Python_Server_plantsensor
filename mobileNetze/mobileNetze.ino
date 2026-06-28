#include <BLEDevice.h>
#include <WiFi.h>
#include "config.h"
#include "sensorStore.h"
#include "sendData.h"

// ─── BLE Service / Characteristic UUIDs (Xiaomi Flora) ─────────────────
static BLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
static BLEUUID uuidWriteMode("00001a00-0000-1000-8000-00805f9b34fb");
static BLEUUID uuidSensorData("00001a01-0000-1000-8000-00805f9b34fb");

// Boot-Zaehler ueberlebt den Deep Sleep
RTC_DATA_ATTR int bootCount = 0;

// Watchdog-Task: erzwingt Schlaf, falls ein Lauf haengen bleibt
TaskHandle_t hibernateTaskHandle = NULL;

WiFiClient espClient;

// ─── Deep Sleep ────────────────────────────────────────────────────────
void hibernate() {
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION * 1000000ULL);
  Serial.println("Going to sleep now.");
  delay(100);
  esp_deep_sleep_start();
}

void delayedHibernate(void* parameter) {
  delay(EMERGENCY_HIBERNATE * 1000);
  Serial.println("Emergency hibernate!");
  hibernate();
}

// ─── WiFi ────────────────────────────────────────────────────────────────
void connectWifi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void disconnectWifi() {
  WiFi.disconnect(true);
  Serial.println("WiFi disconnected");
}

// ─── BLE ─────────────────────────────────────────────────────────────────
BLEClient* getFloraClient(BLEAddress address) {
  BLEClient* client = BLEDevice::createClient();
  if (!client->connect(address)) {
    Serial.println("- Connection failed, skipping");
    return nullptr;
  }
  Serial.println("- Connection successful");
  return client;
}

BLERemoteService* getFloraService(BLEClient* client) {
  BLERemoteService* service = nullptr;
  try {
    service = client->getService(serviceUUID);
  } catch (...) {}
  if (service == nullptr) {
    Serial.println("- Failed to find data service");
  } else {
    Serial.println("- Found data service");
  }
  return service;
}

// Versetzt den Sensor in den Daten-Modus (sonst nur Echtzeit-Flags lesbar).
bool forceDataMode(BLERemoteService* service) {
  Serial.println("- Force device in data mode");
  BLERemoteCharacteristic* characteristic = nullptr;
  try {
    characteristic = service->getCharacteristic(uuidWriteMode);
  } catch (...) {}
  if (characteristic == nullptr) {
    Serial.println("-- Failed, skipping device");
    return false;
  }
  uint8_t cmd[2] = { 0xA0, 0x1F };
  characteristic->writeValue(cmd, sizeof(cmd), true);
  delay(500);
  return true;
}

// Liest die Sensordaten-Characteristic, parst sie und sendet sie an den Server.
bool readSensorData(BLERemoteService* service, const char* mac) {
  Serial.println("- Access characteristic from device");
  BLERemoteCharacteristic* characteristic = nullptr;
  try {
    characteristic = service->getCharacteristic(uuidSensorData);
  } catch (...) {}
  if (characteristic == nullptr) {
    Serial.println("-- Failed, skipping device");
    return false;
  }

  Serial.println("- Read value from characteristic");
  String value;
  try {
    value = characteristic->readValue();
  } catch (...) {
    Serial.println("-- Read failed, skipping device");
    return false;
  }

  if (value.length() < 10) {
    Serial.println(F("-- Too short, skipping device"));
    return false;
  }

  // Byte-Layout der Flora-Sensordaten (Little-Endian)
  float temperature  = (((uint8_t)value[1] << 8) | (uint8_t)value[0]) / 10.0f;
  int   light        =  ((uint8_t)value[4] << 8) | (uint8_t)value[3];
  int   moisture     =   (uint8_t)value[7];
  int   conductivity =  ((uint8_t)value[9] << 8) | (uint8_t)value[8];

  Serial.printf("Temp: %.1f°C  Licht: %d lux  Feuchte: %d%%  Leitf.: %d µS/cm\n",
                temperature, light, moisture, conductivity);

  // Plausibilitaetspruefung gegen korrupte BLE-Reads
  if (temperature > 200 || temperature < -30 || conductivity > 3000) {
    Serial.println("-- Unreasonable values, skip publish");
    return false;
  }

  postToServer(espClient, mac, temperature, light, moisture, conductivity);
  return true;
}

// Verbindet, liest und trennt einen einzelnen Sensor.
bool processFloraDevice(BLEAddress address, const char* mac, int tryCount) {
  Serial.printf("Processing Flora at %s (try %d)\n", address.toString().c_str(), tryCount);

  BLEClient* client = getFloraClient(address);
  if (client == nullptr) return false;

  BLERemoteService* service = getFloraService(client);
  bool success = false;
  if (service != nullptr && forceDataMode(service)) {
    success = readSensorData(service, mac);
  }

  client->disconnect();
  
  return success;
}

// ─── Setup / Loop ────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  bootCount++;
  Serial.printf("Boot #%d\n", bootCount);

  // Watchdog starten, der einen haengenden Lauf hart beendet
  xTaskCreate(delayedHibernate, "hibernate", 4096, NULL, 1, &hibernateTaskHandle);

  Serial.println("Initialize BLE client...");
  BLEDevice::init("");
  BLEDevice::setPower(ESP_PWR_LVL_P7);

  connectWifi();
  loadSensors();

  for (size_t i = 0; i < sensorCount; i++) {
    const char* mac = sensorMacs[i];
    BLEAddress address(mac);
    for (int tryCount = 1; tryCount <= RETRY; tryCount++) {
      if (processFloraDevice(address, mac, tryCount)) {
        delay(1000);
        break;
      }
      delay(1000);
    }
    delay(1000);
  }

  disconnectWifi();
  vTaskDelete(hibernateTaskHandle);
  hibernate();
}

void loop() {
  // Wird nie erreicht: setup() endet immer im Deep Sleep.
  delay(10000);
}
