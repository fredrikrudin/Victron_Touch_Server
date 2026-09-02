#ifndef BUS_CONFIG_H
#define BUS_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

struct BusSettings {
    int rs485Baud;
    int canSpeedKbps;
};

extern BusSettings busSettings;
void initRS485(); 
void initCAN();

inline void loadBusSettings() {
    Preferences prefs;
    prefs.begin("bus_cfg", false);
    busSettings.rs485Baud = prefs.getInt("baud", 9600);
    busSettings.canSpeedKbps = prefs.getInt("can_speed", 500);
    prefs.end();
}

inline void saveBusSettings() {
    Preferences prefs;
    prefs.begin("bus_cfg", false);
    prefs.putInt("baud", busSettings.rs485Baud);
    prefs.putInt("can_speed", busSettings.canSpeedKbps);
    prefs.end();
}

inline void factoryResetBusSettings() {
    Preferences prefs;
    prefs.begin("bus_cfg", false);
    prefs.clear(); 
    prefs.end();
    
    busSettings.rs485Baud = 9600;
    busSettings.canSpeedKbps = 500;
    
    initRS485();
    initCAN();
}

#endif
