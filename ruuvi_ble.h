#ifndef RUUVI_BLE_H
#define RUUVI_BLE_H

#include "config.h"
#include "clock_manager.h"
#include <BLEDevice.h>

inline void decodeRuuviV5(RuuviTag& tag, uint8_t* data, int len) {
    if (len < 24) return; 

    String currentTime = getFormattedTime();
    strncpy(tag.lastSeen, currentTime.c_str(), sizeof(tag.lastSeen) - 1);

    int16_t tempRaw = (data[3] << 8) | data[4];
    tag.temperature = tempRaw * 0.005;

    uint16_t humidRaw = (data[5] << 8) | data[6];
    tag.humidity = humidRaw * 0.0025;
}

inline void initVictronAndRuuviBLE() {
    BLEScan* pBLEScan = BLEDevice::getBLEScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

inline void runVictronBLEScan() {
    BLEScan* pBLEScan = BLEDevice::getBLEScan();
    BLEScanResults foundDevices = pBLEScan->start(1, false);
    
    for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        if (device.haveManufacturerData()) {
            std::string mData = device.getManufacturerData();
            uint8_t* rawData = (uint8_t*)mData.data();
            uint16_t manuID = (rawData[1] << 8) | rawData[0];

            if (manuID == 0x02D8) { // Victron ID
                String mac = device.getAddress().toString().c_str();
                for (int j = 0; j < victronCount; j++) {
                    if (strcasecmp(savedVictrons[j].mac, mac.c_str()) == 0) {
                        uint8_t decrypted[32] = {0};
                        uint8_t recordType = rawData[2];
                        decryptVictronPacket(rawData, mData.length(), savedVictrons[j].encryptionKey, decrypted);
                        
                        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                            savedVictrons[j].connected = true;
                            parseVictronData(savedVictrons[j], decrypted, recordType);
                            xSemaphoreGive(dataMutex);
                        }
                    }
                }
            }
        }
    }
    pBLEScan->clearResults();
}

inline void runRuuviBLEScan() {
    BLEScan* pBLEScan = BLEDevice::getBLEScan();
    BLEScanResults foundDevices = pBLEScan->start(1, false);

    for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        if (device.haveManufacturerData()) {
            std::string mData = device.getManufacturerData();
            uint8_t* rawData = (uint8_t*)mData.data();
            uint16_t manuID = (rawData[1] << 8) | rawData[0];

            if (manuID == 0x0499) { // Ruuvi ID
                String mac = device.getAddress().toString().c_str();
                for (int j = 0; j < ruuviCount; j++) {
                    if (strcasecmp(savedRuuvis[j].mac, mac.c_str()) == 0) {
                        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                            savedRuuvis[j].active = true;
                            decodeRuuviV5(savedRuuvis[j], rawData, mData.length());
                            xSemaphoreGive(dataMutex);
                        }
                    }
                }
            }
        }
    }
    pBLEScan->clearResults();
}

#endif
