#ifndef CLOCK_MANAGER_H
#define CLOCK_MANAGER_H

#include <Wire.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"

#define PCF85063_ADDR 0x51

// Konfigurera och aktivera tidzon i ESP32:s interna operativsystem
inline void applyTimeZone() {
    if (sysSettings.useSwedenTZ) {
        // POSIX-sträng för Sverige: CET (Central European Time) UTC+1, 
        // CEST (Central European Summer Time) UTC+2 från sista söndagen i mars (M3.5.0) 
        // till sista söndagen i oktober (M10.5.0) kl 03:00.
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();
        Serial.println("[CLOCK] Tidzon satt till: Sverige (Automatisk sommar/vintertid)");
    } else {
        // Fallback till ren manuell UTC-offset utan sommartid
        char tzBuf[32];
        snprintf(tzBuf, sizeof(tzBuf), "GMT%+d", -(sysSettings.manualUtcOffset / 3600));
        setenv("TZ", tzBuf, 1);
        tzset();
        Serial.printf("[CLOCK] Manuell UTC-tidzon applicerad: %s\n", tzBuf);
    }
}

inline void initClock() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    Wire.beginTransmission(PCF85063_ADDR);
    Wire.write(0x04); 
    if (Wire.endTransmission() == 0) {
        Wire.requestFrom(PCF85063_ADDR, 7);
        uint8_t sec  = bcdToDec(Wire.read() & 0x7F);
        uint8_t min  = bcdToDec(Wire.read() & 0x7F);
        uint8_t hour = bcdToDec(Wire.read() & 0x3F);
        uint8_t day  = bcdToDec(Wire.read() & 0x3F);
        Wire.read(); 
        uint8_t month = bcdToDec(Wire.read() & 0x1F);
        int year = bcdToDec(Wire.read()) + 2000;

        struct tm tm;
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
        
        time_t t = mktime(&tm);
        struct timeval now = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&now, NULL);
        
        applyTimeZone(); // Se till att tidzonen läggs på klockvärdet från hårdvaru-RTC
    }
}

// Kontrollerar om sommartid (DST) är aktiv just nu i Sverige
inline bool isDSTActive() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    return (timeinfo.tm_isdst > 0);
}

#endif
