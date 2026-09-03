#ifndef INDUSTRIAL_BUSSES_H
#define INDUSTRIAL_BUSSES_H

#include <Arduino.h>       // <--- SAKNADES! Ger tillgång till millis(), analogRead, SERIAL_8N1 etc.
#include "config.h"
#include "bus_config.h" 
#include "clock_manager.h"
#include <driver/twai.h>

// --- GLOBALA VARIABLER (Hämtas från config/main) ---
extern HardwareSerial RS485Serial;
extern bool isRelayPhysicallyConnected;

// --- SAKNADES: Berätta för kompilatorn att dessa structs finns i projektet ---
extern BusSettings busSettings;   // Gör att busSettings.rs485Baud kan läsas
extern SysSettings sysSettings;   // Gör att sysSettings.autoRelayCheck kan läsas

// --- HJÄLPFUNKTIONER (Modbus) ---
inline uint16_t calculateCRC(uint8_t *buffer, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

inline void sendModbusCommand(uint8_t slaveID, uint8_t functionCode, uint16_t reg, uint16_t value) {
    uint8_t packet[8];
    packet[0] = slaveID;
    packet[1] = functionCode;
    packet[2] = (reg >> 8) & 0xFF;
    packet[3] = reg & 0xFF;
    packet[4] = (value >> 8) & 0xFF;
    packet[5] = value & 0xFF;
    
    uint16_t crc = calculateCRC(packet, 6);
    packet[6] = crc & 0xFF;
    packet[7] = (crc >> 8) & 0xFF;

    RS485Serial.write(packet, 8);
    RS485Serial.flush();
}

// --- INITIERINGAR ---
inline void initRS485() {
    RS485Serial.begin(busSettings.rs485Baud, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
}

inline void initCAN() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_timing_config_t t_config;

    if (busSettings.canSpeedKbps == 250)      t_config = TWAI_TIMING_CONFIG_250KBITS();
    else if (busSettings.canSpeedKbps == 125) t_config = TWAI_TIMING_CONFIG_125KBITS();
    else                                      t_config = TWAI_TIMING_CONFIG_500KBITS();

    twai_driver_uninstall(); 
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        twai_start();
    }
}

// --- RELÄKOLL ---
inline void checkRelayPresence() {
    if (!sysSettings.autoRelayCheck) {
        isRelayPhysicallyConnected = true;
        return;
    }
    
    uint8_t pingPacket[] = { 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x7D, 0xCA }; 

    while(RS485Serial.available()) RS485Serial.read();

    RS485Serial.write(pingPacket, 8);
    RS485Serial.flush();

    unsigned long startWait = millis();
    bool response = false;
    while (millis() - startWait < 100) {
        if (RS485Serial.available() >= 5) {
            response = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    isRelayPhysicallyConnected = response;
}

// --- BATTERIMÄTNING ---
inline float readBatteryVoltage() {
    int raw = analogRead(BAT_ADC_PIN);
    return ((raw / 4095.0) * 3.3) * 2.0; 
}

#endif
