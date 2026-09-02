#include "config.h"

VictronDevice savedVictrons[MAX_DEVICES];
int victronCount = 0;
RuuviTag savedRuuvis[MAX_DEVICES];
int ruuviCount = 0;
SystemSettings sysSettings;

SemaphoreHandle_t dataMutex = NULL;
SemaphoreHandle_t lvglMutex = NULL;

void loadAllSettings() {
    Preferences prefs;
    prefs.begin("sys_cfg", false);
    
    if (!prefs.getBytes("sys", &sysSettings, sizeof(SystemSettings))) {
        strcpy(sysSettings.ntpServer, "pool.ntp.org");
        sysSettings.victronScanInterval = 5;
        sysSettings.ruuviScanInterval = 15;
        sysSettings.brightnessDay = 90;
        sysSettings.brightnessNight = 20;
        sysSettings.brightnessDimmed = 10;
        sysSettings.screenTimeoutSec = 30;
        sysSettings.nightStartHour = 22;
        sysSettings.nightEndHour = 6;
        sysSettings.autoRelayCheck = true;
        prefs.putBytes("sys", &sysSettings, sizeof(SystemSettings));
    }
    
    victronCount = prefs.getInt("v_count", 0);
    if(victronCount > 0) prefs.getBytes("victrons", savedVictrons, sizeof(savedVictrons));
    
    ruuviCount = prefs.getInt("r_count", 0);
    if(ruuviCount > 0) prefs.getBytes("ruuvis", savedRuuvis, sizeof(savedRuuvis));
    
    prefs.end();
}

void saveAllSettings() {
    Preferences prefs;
    prefs.begin("sys_cfg", false);
    prefs.putBytes("sys", &sysSettings, sizeof(SystemSettings));
    prefs.putInt("v_count", victronCount);
    if(victronCount > 0) prefs.getBytes("victrons", savedVictrons, sizeof(savedVictrons));
    prefs.putInt("r_count", ruuviCount);
    if(ruuviCount > 0) prefs.getBytes("ruuvis", savedRuuvis, sizeof(savedRuuvis));
    prefs.end();
}

void factoryResetBLESettings() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Preferences prefs;
        prefs.begin("sys_cfg", false);
        prefs.remove("v_count");
        prefs.remove("victrons");
        prefs.remove("r_count");
        prefs.remove("ruuvis");
        
        sysSettings.victronScanInterval = 5;
        sysSettings.ruuviScanInterval = 15;
        prefs.putBytes("sys", &sysSettings, sizeof(SystemSettings));
        prefs.end();
        
        victronCount = 0;
        ruuviCount = 0;
        memset(savedVictrons, 0, sizeof(savedVictrons));
        memset(savedRuuvis, 0, sizeof(savedRuuvis));
        
        xSemaphoreGive(dataMutex);
    }
}
