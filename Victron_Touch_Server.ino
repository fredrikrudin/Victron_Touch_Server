/**
 * ====================================================================
 *                         CABBY v0.8
 *       Industrial Smart Gateway for Waveshare ESP32-S3-Touch-LCD-4
 * ====================================================================
 */

#include "config.h"
#include "ui.h"
#include "industrial_busses.h"
#include "clock_manager.h"
#include "web_server.h"
#include "ruuvi_ble.h"
#include "victron_ble.h"
#include <Wire.h>
#include <WiFi.h>

HardwareSerial RS485Serial(1);
String currentSessionToken = "";
unsigned long sessionTimeout = 0;
unsigned long lastTouchTime = 0;

TaskHandle_t RadioTaskHandle = NULL;

// SMART WIFI-INITIERING: ANANSKUTER TILL HOME-STA ELLER AP FALLBACK
void initWiFiNetwork() {
    WiFi.disconnect(true);
    delay(100);
    
    if (sysSettings.useSTA && strlen(sysSettings.wifiSSID) > 0) {
        Serial.printf("[WiFi] Ansluter till nätverk: %s...\n", sysSettings.wifiSSID);
        WiFi.mode(WIFI_AP_STA); // Kör dubbla lägen för driftsäkerhet
        WiFi.begin(sysSettings.wifiSSID, sysSettings.wifiPass);
        
        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 20) {
            delay(500);
            Serial.print(".");
            timeout++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("\n[WiFi] ANSLUTEN! Lokal IP: ");
            Serial.println(WiFi.localIP());
            configTime(3600, 3600, sysSettings.ntpServer); // Synka klockan direkt över internet
            return;
        } else {
            Serial.println("\n[WiFi] Router saknas. Faller tillbaka på intern Access Point.");
        }
    }
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CABBY_Gateway_AP", "12345678");
    Serial.print("[WiFi] Lokal Access Point startad. IP: ");
    Serial.println(WiFi.softAPIP());
}

// ASYNKRON RADIO-WORKER PÅ PROCESSORKÄRNA 0 (WiFi, BLE, Web, Krypto)
void radioWorkerTask(void * pvParameters) {
    initWiFiNetwork();
    initWebServer();
    initVictronAndRuuviBLE();
    
    unsigned long lastVictron = 0;
    unsigned long lastRuuvi = 0;
    
    for(;;) {
        handleWebServer();
        unsigned long currentMillis = millis();
        
        if (currentMillis - lastVictron > (unsigned long)(sysSettings.victronScanInterval * 1000)) {
            runVictronBLEScan(); 
            lastVictron = currentMillis;
        }
        
        if (currentMillis - lastRuuvi > (unsigned long)(sysSettings.ruuviScanInterval * 1000)) {
            runRuuviBLEScan();
            lastRuuvi = currentMillis;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void initWaveshareKretskort() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // Slå på strömmen till Modbus och CAN-transceivers via den inbyggda expandern
    Wire.beginTransmission(0x20);
    Wire.write(0x03);             // Konfigurationsregister
    Wire.write(0x00);             // Utgångar
    Wire.endTransmission();

    Wire.beginTransmission(0x20);
    Wire.write(0x01);             // Utgångsregister
    Wire.write(0xFF);             // Aktivera alla transceivers
    Wire.endTransmission();
    Serial.println("[SYSTEM] Waveshare IO-Expander synkad. Hårdvarubussar strömsatta.");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- BOOTING CABBY v0.8 ENGINE ---");
    
    dataMutex = xSemaphoreCreateMutex();
    lvglMutex = xSemaphoreCreateMutex();
    
    initWaveshareKretskort(); 
    loadAllSettings();
    loadBusSettings();
    initClock();
    initRS485();
    initCAN();
    
    initDisplayAndTouch(); 

    // Skjut ut nätverkstrafiken till Kärna 0
    xTaskCreatePinnedToCore(radioWorkerTask, "RadioWorker", 8192, NULL, 1, &RadioTaskHandle, 0);
}

void loop() {
    // --- PROCESSORKÄRNA 1 (CORE 1): GRAFIK, TOUCH OCH TRÅDADE BUSSAR ---
    if (xSemaphoreTake(lvglMutex, pdMS_TO_TICKS(16)) == pdTRUE) {
        updateUI(); 
        xSemaphoreGive(lvglMutex);
    }
    
    pollRS485();
    pollCAN();
    
    static unsigned long lastRelayGuard = 0;
    if (millis() - lastRelayGuard > 10000) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            checkRelayPresence();
            xSemaphoreGive(dataMutex);
        }
        lastRelayGuard = millis();
    }
    
    static unsigned long lastSystemPrint = 0;
    if (millis() - lastSystemPrint > 30000) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            float vBat = readBatteryVoltage();
            Serial.printf("[CABBY v0.8] IP: %s | LiPo: %.2f V | RS485: %d bps\n", 
                          sysSettings.useSTA ? WiFi.localIP().toString().c_str() : WiFi.softAPIP().toString().c_str(), 
                          vBat, busSettings.rs485Baud);
            xSemaphoreGive(dataMutex);
        }
        lastSystemPrint = millis();
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
}
