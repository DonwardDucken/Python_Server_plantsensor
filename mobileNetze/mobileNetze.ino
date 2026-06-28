#include <BLEDevice.h>
#include <WiFi.h>
#include "config.h"
#include "SPI.h"
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>
#include "sendData.h"

RTC_DATA_ATTR int bootCount = 0;
static int deviceCount = sizeof FLORA_DEVICES / sizeof FLORA_DEVICES[0];

static BLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
static BLEUUID uuid_version_battery("00001a02-0000-1000-8000-00805f9b34fb");
static BLEUUID uuid_sensor_data("00001a01-0000-1000-8000-00805f9b34fb");
static BLEUUID uuid_write_mode("00001a00-0000-1000-8000-00805f9b34fb");

TaskHandle_t hibernateTaskHandle = NULL;
WiFiClient espClient;

void connectWifi() {
  //Serial.printf("local_ip: %s, Gateway: %s, Subnet: %s", local_IP.toString(), gateway.toString(), subnet.toString());
  //WiFi.config(local_IP, gateway, subnet); //-> DHCP
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

BLEClient* getFloraClient(BLEAddress floraAddress) {
  BLEClient* floraClient = BLEDevice::createClient();
  if (!floraClient->connect(floraAddress)) {
    Serial.println("- Connection failed, skipping");
    return nullptr;
  }
  Serial.println("- Connection successful");
  return floraClient;
}

BLERemoteService* getFloraService(BLEClient* floraClient) {
  BLERemoteService* floraService = nullptr;
  try {
    floraService = floraClient->getService(serviceUUID);
  } catch (...) {}
  if (floraService == nullptr) {
    Serial.println("- Failed to find data service");
  } else {
    Serial.println("- Found data service");
  }
  return floraService;
}

bool forceFloraServiceDataMode(BLERemoteService* floraService) {
  BLERemoteCharacteristic* floraCharacteristic = nullptr;
  Serial.println("- Force device in data mode");
  try {
    floraCharacteristic = floraService->getCharacteristic(uuid_write_mode);
  } catch (...) {}
  if (floraCharacteristic == nullptr) {
    Serial.println("-- Failed, skipping device");
    return false;
  }
  uint8_t buf[2] = { 0xA0, 0x1F };
  floraCharacteristic->writeValue(buf, 2, true);
  delay(500);
  return true;
}

bool readFloraDataCharacteristic(BLERemoteService* floraService, char* deviceMacAddress) {
  BLERemoteCharacteristic* floraCharacteristic = nullptr;
  Serial.println("- Access characteristic from device");
  try {
    floraCharacteristic = floraService->getCharacteristic(uuid_sensor_data);
  } catch (...) {}
  if (floraCharacteristic == nullptr) {
    Serial.println("-- Failed, skipping device");
    return false;
  }

  Serial.println("- Read value from characteristic");
  String value;
  try {
    value = floraCharacteristic->readValue();
  } catch (...) {
    Serial.println("-- Failed, skipping device");
    return false;
  }

  if (value.length() < 10) {
    Serial.println(F("-- Too short, skipping device"));
    return false;
  }

  // Byte-Zugriff per uint8_t cast
  float temperature = (((uint8_t)value[1] << 8) | (uint8_t)value[0]) / 10.0;
  int light = ((uint8_t)value[4] << 8) | (uint8_t)value[3];
  int moisture = (uint8_t)value[7];
  int conductivity = ((uint8_t)value[9] << 8) | (uint8_t)value[8];

  Serial.printf("Temp: %.1f°C  Licht: %d lux  Feuchte: %d%%  Leitf.: %d µS/cm\n",
                temperature, light, moisture, conductivity);

  if (temperature > 200 || temperature < -30 || conductivity > 3000) {
    Serial.println("-- Unreasonable values, skip publish");
    return false;
  }

  postToServer(temperature, light, moisture, conductivity, deviceMacAddress, espClient);

  return true;
}
/*
bool readFloraBatteryCharacteristic(BLERemoteService* floraService) {
  BLERemoteCharacteristic* floraCharacteristic = nullptr;
  Serial.println("- Access battery characteristic from device");
  try {
    floraCharacteristic = floraService->getCharacteristic(uuid_version_battery);
  } catch (...) {}
  if (floraCharacteristic == nullptr) {
    Serial.println("-- Failed, skipping battery level");
    return false;
  }

  Serial.println("- Read value from characteristic");
  String value;  // <-- HIER: ausserhalb try, kein Scope-Bug
  try {
    value = floraCharacteristic->readValue();
  } catch (...) {
    Serial.println("-- Failed, skipping battery level");
    return false;
  }

  if (value.length() < 1) {
    Serial.println("-- Too short");
    return false;
  }

  int battery = (uint8_t)value[0];
  Serial.print("-- Battery: ");
  Serial.println(battery);

  char buffer[64];
  snprintf(buffer, 64, "%d", battery);
  return true;
}*/

bool processFloraService(BLERemoteService* floraService, char* deviceMacAddress, bool readBattery) {
  if (!forceFloraServiceDataMode(floraService)) return false;

  bool dataSuccess = readFloraDataCharacteristic(floraService, deviceMacAddress);
  bool batterySuccess = true;
  /*if (readBattery) {
    batterySuccess = readFloraBatteryCharacteristic(floraService);
  }*/
  return dataSuccess && batterySuccess;
}

bool processFloraDevice(BLEAddress floraAddress, char* deviceMacAddress, bool getBattery, int tryCount) {
  Serial.printf("Processing Flora at %s (try %d)\n", floraAddress.toString().c_str(), tryCount);
  BLEClient* floraClient = getFloraClient(floraAddress);
  if (floraClient == nullptr) return false;
  BLERemoteService* floraService = getFloraService(floraClient);
  if (floraService == nullptr) {
    floraClient->disconnect();
    return false;
  }
  bool success = processFloraService(floraService, deviceMacAddress, getBattery);
  floraClient->disconnect();
  return success;
}

void hibernate() {
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION * 1000000ll);
  Serial.println("Going to sleep now.");
  delay(100);
  esp_deep_sleep_start();
}

void delayedHibernate(void* parameter) {
  delay(EMERGENCY_HIBERNATE * 1000);
  Serial.println("Emergency hibernate!");
  hibernate();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  bootCount++;
  xTaskCreate(delayedHibernate, "hibernate", 4096, NULL, 1, &hibernateTaskHandle);

  Serial.println("Initialize BLE client...");
  BLEDevice::init("");
  BLEDevice::setPower(ESP_PWR_LVL_P7);

  connectWifi();
  
  bool readBattery = ((bootCount % BATTERY_INTERVAL) == 0);

  for (int i = 0; i < deviceCount; i++) {
    int tryCount = 0;
    char* deviceMacAddress = FLORA_DEVICES[i];
    //postToServer(0,0,0,0,deviceMacAddress, espClient);
    BLEAddress floraAddress(deviceMacAddress);
    while (tryCount < RETRY) {
      tryCount++;
      if (processFloraDevice(floraAddress, deviceMacAddress, readBattery, tryCount)) {
        delay(1000);
        break;
      }
      delay(1000);
    }
    delay(1000);
  };
  disconnectWifi();
  vTaskDelete(hibernateTaskHandle);
  hibernate();
}

void loop() {
  delay(10000);
}