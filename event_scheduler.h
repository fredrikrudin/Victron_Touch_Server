#ifndef EVENT_SCHEDULER_H
#define EVENT_SCHEDULER_H

#include "config.h"
#include "industrial_busses.h"
#include <time.h>

// Allokera variabler (Hanteras globalt via config/NVS)
ScheduleEvent savedEvents[MAX_EVENTS];
int eventCount = 0;

/**
 * Beräknar standard Modbus CRC16-kontrollsumma (XOR 0xA001)
 * Krävs för att industriella RS485-reläer ska godkänna kommandot.
 */
uint16_t calculateModbusCRC(uint8_t *buffer, uint16_t length) {
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

/**
 * Konstruerar och skickar ett fysiskt Modbus RTU-paket över Waveshares RS485-buss.
 * Standard Function Code 0x05 används för att tända/släcka Coils (reläer).
 */
void executeRS485RelayCommand(uint8_t slaveID, uint16_t coilRegister, uint16_t actionValue) {
    uint8_t modbusPacket[8];
    
    modbusPacket[0] = slaveID;          // Slavadress (1-247)
    modbusPacket[1] = 0x05;             // Function Code 0x05: Write Single Coil
    modbusPacket[2] = (coilRegister >> 8) & 0xFF; // Register Hög byte
    modbusPacket[3] = coilRegister & 0xFF;        // Register Låg byte
    modbusPacket[4] = (actionValue >> 8) & 0xFF;   // Värde Hög byte (0xFF00 = PÅ, 0x0000 = AV)
    modbusPacket[5] = actionValue & 0xFF;          // Värde Låg byte
    
    // Beräkna och lägg till CRC-kontrollsumman (Little Endian för Modbus)
    uint16_t crc = calculateModbusCRC(modbusPacket, 6);
    modbusPacket[6] = crc & 0xFF;        // CRC Låg
    modbusPacket[7] = (crc >> 8) & 0xFF; // CRC Hög

    // Skriv rådatat direkt till den verifierade RS485-porten (UART1)
    RS485Serial.write(modbusPacket, 8);
    RS485Serial.flush(); // Vänta tills hela paketet har skickats fysiskt

    // Utförlig felsökningslogg till Serial Monitor
    Serial.println("\n>>> [SCHEDULER EXECUTION] <<<");
    Serial.printf("[RS485 OUT] Modbus-paket skickat till Slav: %d\n", slaveID);
    Serial.printf("[RS485 OUT] Register/Coil: %d | Åtgärd: 0x%04X\n", coilRegister, actionValue);
    Serial.print("[RS485 OUT] Råa Bytes ut: ");
    for(int i = 0; i < 8; i++) {
        Serial.printf("%02X ", modbusPacket[i]);
    }
    Serial.println("\n------------------------------");
}

/**
 * Initierar schemaläggaren vid boot och nollställer temporära flaggor.
 */
void initScheduler() {
    for (int i = 0; i < eventCount; i++) {
        savedEvents[i].triggeredToday = false;
    }
    Serial.printf("[SCHEDULER] Initierad. %d aktiva scheman laddade från minnet.\n", eventCount);
}

/**
 * Kontrollerar sparade händelser mot systemklockan.
 * Bör köras i huvudloopen en gång per sekund.
 */
void checkAndExecuteSchedules() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Återställ 'triggeredToday'-flaggorna precis vid midnatt så de kan köras nästa dag
    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        for (int i = 0; i < eventCount; i++) {
            savedEvents[i].triggeredToday = false;
        }
        Serial.println("[SCHEDULER] Midnatt nådd. Alla dagliga händelseflaggor har återställts.");
    }

    // Gå igenom händelselistan
    for (int i = 0; i < eventCount; i++) {
        if (!savedEvents[i].active) continue;

        // Kontrollera om timme och minut matchar det sparade eventet
        if (timeinfo.tm_hour == savedEvents[i].hour && timeinfo.tm_min == savedEvents[i].minute) {
            
            // Kontrollera att det inte redan har körts under denna minut
            if (!savedEvents[i].triggeredToday) {
                
                Serial.printf("[SCHEDULER] MATCH: Eventet '%s' triggades av klockan (%02d:%02d)\n", 
                              savedEvents[i].label, savedEvents[i].hour, savedEvents[i].minute);
                
                // Kör hårdvarukommandot
                executeRS485RelayCommand(savedEvents[i].modbusSlaveID, 
                                         savedEvents[i].modbusRegister, 
                                         savedEvents[i].valueToSend);
                
                // Sätt flaggan så att vi inte skickar paketet 60 gånger under samma minut
                savedEvents[i].triggeredToday = true;
            }
        }
    }
}

#endif
