#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "bus_config.h"

// Hårdvaru-pinnar verifierade mot Waveshare v4 källkod
#define I2C_SDA_PIN   11
#define I2C_SCL_PIN   12
#define BAT_ADC_PIN   4
#define BACKLIGHT_PIN 1

// BLE Enhetstyper för Victron
enum VictronType { TYPE_UNKNOWN, TYPE_SOLAR_CHARGER, TYPE_BATTERY_MONITOR, TYPE_INVERTER, TYPE_DCDC };

struct VictronDevice {
    char mac;             // SAKNAR STORLEK! Rymmer bara 1 bokstav
    char name;            // SAKNAR STORLEK!
    char encryptionKey;   // SAKNAR STORLEK!
    // ...
    char lastSeen;        // SAKNAR STORLEK!
};

struct RuuviTag {
    char mac;             // SAKNAR STORLEK!
    char name;            // SAKNAR STORLEK!
    // ...
    char lastSeen;        // SAKNAR STORLEK!
};

struct SystemSettings {
    char ntpServer;       // SAKNAR STORLEK!
    // ...
    char wifiSSID;        // SAKNAR STORLEK!
    char wifiPass;        // SAKNAR STORLEK!
};

struct DiscoveredBLE {
    char mac;             // SAKNAR STORLEK!
    int rssi;
};


#define MAX_DEVICES 5
#define MAX_DISCOVERED 10

extern VictronDevice savedVictrons[MAX_DEVICES];
extern int victronCount;
extern RuuviTag savedRuuvis[MAX_DEVICES];
extern int ruuviCount;
extern SystemSettings sysSettings;

// Temporärt hittade enheter vid manuell skanning
extern DiscoveredBLE discVictrons[MAX_DISCOVERED];
extern int discVictronCount;

// Trådsäkra lås (Mutex)
extern SemaphoreHandle_t dataMutex;
extern SemaphoreHandle_t lvglMutex;

void loadAllSettings();
void saveAllSettings();
void factoryResetBLESettings();

#endif
