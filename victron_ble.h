#ifndef VICTRON_BLE_H
#define VICTRON_BLE_H

#include "config.h"
#include "clock_manager.h"
#include <BLEDevice.h>
#include <mbedtls/aes.h>

inline void hexStringToBytes(const char* hex, uint8_t* bytes) {
    for (size_t i = 0; i < 16; i++) {
        char str[3] = { hex[i*2], hex[i*2+1], '\0' };
        bytes[i] = (uint8_t)strtol(str, NULL, 16);
    }
}

inline void decryptVictronPacket(uint8_t* encryptedData, size_t dataLen, const char* keyStr, uint8_t* decryptedOutput) {
    uint8_t key[16];
    hexStringToBytes(keyStr, key);

    uint8_t iv[16] = {0};
    iv[0] = encryptedData[2]; 
    iv[1] = encryptedData[3]; 
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);

    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};

    mbedtls_aes_crypt_ctr(&aes, dataLen - 6, &nc_off, iv, stream_block, encryptedData + 6, decryptedOutput);
    mbedtls_aes_free(&aes);
}

inline const char* getVictronErrorString(uint8_t errorCode) {
    switch (errorCode) {
        case 0:   return "OK";
        case 2:   return "Overspanning Batteri";
        case 17:  return "Laddare Overhettad";
        case 19:  return "Overstrom";
        case 33:  return "PV-spanning for hog";
        default:  return "Okant fel/Varning";
    }
}

inline void parseVictronData(VictronDevice& dev, uint8_t* decryptedPayload, uint8_t recordType) {
    String currentTime = getFormattedTime();
    strncpy(dev.lastSeen, currentTime.c_str(), sizeof(dev.lastSeen) - 1);

    switch (recordType) {
        case 0x01: 
            dev.type = TYPE_SOLAR_CHARGER;
            dev.deviceState = decryptedPayload[0]; 
            dev.chargerError = decryptedPayload[1];
            dev.batteryVoltage = ((decryptedPayload[3] << 8) | decryptedPayload[2]) / 100.0;
            dev.batteryCurrent = ((decryptedPayload[5] << 8) | decryptedPayload[4]) / 10.0;
            dev.pvPower = (decryptedPayload[7] << 8) | decryptedPayload[6]; 
            break;

        case 0x02: 
            dev.type = TYPE_BATTERY_MONITOR;
            dev.stateOfCharge = ((decryptedPayload[1] << 8) | decryptedPayload[0]) / 10; 
            dev.batteryVoltage = ((decryptedPayload[3] << 8) | decryptedPayload[2]) / 100.0;
            
            int16_t rawCurrent = (decryptedPayload[5] << 8) | decryptedPayload[4];
            dev.batteryCurrent = rawCurrent / 100.0; 
            dev.chargerError = 0;
            break;
    }
}

// NYTT: MANUELL BLE-SÖKNING UTAN PARNINGSKRAV FOR DISPLAYEN
inline void runVictronDiscoveryScan() {
    BLEScan* pBLEScan = BLEDevice::getBLEScan();
    pBLEScan->setActiveScan(true);
    BLEScanResults foundDevices = pBLEScan->start(2, false); 
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        discVictronCount = 0;
        for (int i = 0; i < foundDevices.getCount(); i++) {
            BLEAdvertisedDevice device = foundDevices.getDevice(i);
            if (device.haveManufacturerData()) {
                std::string mData = device.getManufacturerData();
                uint8_t* rawData = (uint8_t*)mData.data();
                uint16_t manuID = (rawData[1] << 8) | rawData[0];
                
                if (manuID == 0x02D8 && discVictronCount < MAX_DISCOVERED) {
                    String mac = device.getAddress().toString().c_str();
                    
                    bool alreadySaved = false;
                    for(int j=0; j<victronCount; j++) {
                        if(strcasecmp(savedVictrons[j].mac, mac.c_str()) == 0) alreadySaved = true;
                    }
                    
                    if(!alreadySaved) {
                        strncpy(discVictrons[discVictronCount].mac, mac.c_str(), sizeof(discVictrons[discVictronCount].mac) - 1);
                        discVictrons[discVictronCount].rssi = device.getRSSI();
                        discVictronCount++;
                    }
                }
            }
        }
        xSemaphoreGive(dataMutex);
    }
    pBLEScan->clearResults();
}

#endif
