#ifndef CLOCK_MANAGER_H
#define CLOCK_MANAGER_H

#include <Wire.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"

#define PCF85063_ADDR 0x51

inline uint8_t bcdToDec(uint8_t val) { return ((val / 16 * 10) + (val % 16)); }
inline uint8_t decToBcd(uint8_t val) { return ((val / 10 * 16) + (val % 10)); }

inline bool checkRTCStatus() {
    Wire.beginTransmission(PCF85063_ADDR);
    return (Wire.endTransmission() == 0);
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
    }
}

inline void writeTimeToHardwareRTC(int year, int month, int day, int hour, int minute, int second) {
    Wire.beginTransmission(PCF85063_ADDR);
    Wire.write(0x04); 
    Wire.write(decToBcd(second));
    Wire.write(decToBcd(minute));
    Wire.write(decToBcd(hour));
    Wire.write(decToBcd(day));
    Wire.write(0x00); 
    Wire.write(decToBcd(month));
    Wire.write(decToBcd(year - 2000));
    Wire.endTransmission();
}

inline void setManualTime(int year, int month, int day, int hour, int minute, int second) {
    struct tm tm;
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    
    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, NULL);
    
    writeTimeToHardwareRTC(year, month, day, hour, minute, second);
}

inline String getFormattedTime() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return String(buf);
}

#endif
