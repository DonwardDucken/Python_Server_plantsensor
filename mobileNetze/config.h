#pragma once

// array of different xiaomi flora MAC addresses
char* FLORA_DEVICES[] = {
    "5C:85:7E:14:48:E9"
};

// sleep between to runs in seconds
#define SLEEP_DURATION 30
// emergency hibernate countdown in seconds
#define EMERGENCY_HIBERNATE 3 * 60
// how often should the battery be read - in run count
#define BATTERY_INTERVAL 6
// how often should a device be retried in a run when something fails
#define RETRY 3

const char*   WIFI_SSID       = "Desktop";
const char*   WIFI_PASSWORD   = "12345678";
const char*   SERVER_URL = "http://192.168.137.1:8080/data";
const char*   SERVER_ADDRESS = "192.168.137.1";
const int   SERVER_PORT = 8080;
