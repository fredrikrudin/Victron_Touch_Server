#include "config.h" // DETTA MÅSTE LIGGA HÖGST UPP!

// 1. Här allokeras (skapas) variablerna fysiskt i minnet en gång för alla
VictronDevice savedVictrons[MAX_VICTRON];
int victronCount = 0;
RuuviTag savedRuuvis[MAX_RUUVI];
int ruuviCount = 0;
SystemSettings sysSettings;              
ScheduleEvent savedEvents[MAX_EVENTS];   
int eventCount = 0;

// 2. Inkludera sedan undermodulerna (de kan nu läsa variablerna ovan via extern)
#include "clock_manager.h"
#include "industrial_busses.h"
#include "event_scheduler.h"
#include "ruuvi_ble.h"
#include "web_server.h"
#include "ui.h"
#include <Preferences.h>

Preferences prefs;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[SYSTEM] Startar Multi-Gateway...");

    loadAllSettings();
    initClock();
    initRS485();
    initCAN();
    initWebServer();
    initDisplayAndTouch();
    initScheduler();
}

void loop() {
    handleWebServer();
    updateUI();
    updateClock();

    // Hantera schemalagda händelser varje sekund
    static unsigned long lastScheduleCheck = 0;
    if (millis() - lastScheduleCheck > 1000) {
        checkAndExecuteSchedules();
        lastScheduleCheck = millis();
    }

    // Bussdiagnostik var 5:e sekund
    static unsigned long lastBusPoll = 0;
    if (millis() - lastBusPoll > 5000) {
        pollRS485();
        pollCAN();
        lastBusPoll = millis();
    }
    delay(1);
}

void loadAllSettings() {
    prefs.begin("sys_config", false);
    prefs.getBytes("sys", &sysSettings, sizeof(SystemSettings));
    
    // Fallback om minnet är tomt (första uppstarten)
    if (sysSettings.rs485Baud != 9600 && sysSettings.rs485Baud != 115200) {
        strcpy(sysSettings.ntpServer, "pool.ntp.org");
        sysSettings.utcOffsetSeconds = 3600;
        sysSettings.useNTP = true;
        sysSettings.rs485Baud = 9600;
        sysSettings.canSpeedKbps = 500;
    }
    
    victronCount = prefs.getInt("v_count", 0);
    prefs.getBytes("victrons", savedVictrons, sizeof(savedVictrons));
    ruuviCount = prefs.getInt("r_count", 0);
    prefs.getBytes("ruuvis", savedRuuvis, sizeof(savedRuuvis));
    eventCount = prefs.getInt("e_count", 0);
    prefs.getBytes("events", savedEvents, sizeof(savedEvents));
    prefs.end();
    Serial.println("[FLASH] Inställningar inlästa.");
}

void saveAllSettings() {
    prefs.begin("sys_config", false);
    prefs.putBytes("sys", &sysSettings, sizeof(SystemSettings));
    prefs.putInt("v_count", victronCount);
    prefs.putBytes("victrons", savedVictrons, sizeof(savedVictrons));
    prefs.putInt("r_count", ruuviCount);
    prefs.putBytes("ruuvis", savedRuuvis, sizeof(savedRuuvis));
    prefs.putInt("e_count", eventCount);
    prefs.putBytes("events", savedEvents, sizeof(savedEvents));
    prefs.end();
    Serial.println("[FLASH] All data sparad permanent.");
}
