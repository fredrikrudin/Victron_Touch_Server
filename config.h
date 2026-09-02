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
    char mac[18];
    char name[32];
    char encryptionKey[33]; // 32 tecken hex + null-terminator
    VictronType type;
    bool connected;
    
    // Telemetri-union/fält
    float batteryVoltage;
    float batteryCurrent;
    float pvPower;        
    float consumedAh;     
    int stateOfCharge;    
    int deviceState;      
    uint8_t chargerError; 
    char lastSeen[20];     // YYYY-MM-DD HH:MM:SS
};

struct RuuviTag {
    char mac[18];
    char name[32];
    float temperature;
    float humidity;
    bool active;
    char lastSeen[20];
};

struct SystemSettings {
    char ntpServer[64];
    int victronScanInterval; 
    int ruuviScanInterval;
    int brightnessDay;
    int brightnessNight;
    int brightnessDimmed;
    int screenTimeoutSec;
    int nightStartHour;
    int nightEndHour;
    bool autoRelayCheck;     
};

#define MAX_DEVICES 5
extern VictronDevice savedVictrons[MAX_DEVICES];
extern int victronCount;
extern RuuviTag savedRuuvis[MAX_DEVICES];
extern int ruuviCount;
extern SystemSettings sysSettings;

// Trådsäkra lås (Mutex)
extern SemaphoreHandle_t dataMutex;
extern SemaphoreHandle_t lvglMutex;

void loadAllSettings();
void saveAllSettings();
void factoryResetBLESettings();

#endif
