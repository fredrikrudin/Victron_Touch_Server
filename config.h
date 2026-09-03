#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Hårdvarustift för Waveshare v4
#define RS485_RX_PIN 43
#define RS485_TX_PIN 44
#define CAN_RX_PIN   41
#define CAN_TX_PIN   42

// 1. Definitioner av alla strukturer
struct VictronDevice {
    char mac[18];          // Plats för "AA:BB:CC:DD:EE:FF" + null-terminator
    char name[32];
    char encryptionKey[33]; // 32 hex-tecken + null
    float voltage;
    float current;
    bool connected;
};

struct RuuviTag {
    char mac[18];
    char name[32];
    float temperature;
    float humidity;
    float pressure;
    bool active;
};

struct SystemSettings {
    char wifiSSID[32];
    char wifiPass[64];
    char ntpServer[64];
    int utcOffsetSeconds;
    bool useNTP;
    int rs485Baud;
    int canSpeedKbps;
};

struct ScheduleEvent {
    bool active;
    int hour;
    int minute;
    uint8_t modbusSlaveID;
    uint16_t modbusRegister;
    uint16_t valueToSend;
    char label[32];
    bool triggeredToday;
};

#define MAX_VICTRON 5
#define MAX_RUUVI 5
#define MAX_EVENTS 10

// 2. Deklarera variablerna som "extern" så att alla .h-filer kan se dem
extern VictronDevice savedVictrons[MAX_VICTRON];
extern int victronCount;

extern RuuviTag savedRuuvis[MAX_RUUVI];
extern int ruuviCount;

extern SystemSettings sysSettings; // Matchar nu exakt!

extern ScheduleEvent savedEvents[MAX_EVENTS];
extern int eventCount;

// Funktionsprototyper
void loadAllSettings();
void saveAllSettings();

#endif
