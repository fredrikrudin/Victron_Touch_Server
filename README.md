# CABBY v0.8 - ESP32-S3 Industrial Smart Gateway

CABBY är en modulär, högpresterande och trådsäker gateway byggd för **Waveshare ESP32-S3-Touch-LCD-4 v4** (480x480 RGB Touch-display). Systemet samlar in data trådlöst via Bluetooth (BLE) samt trådat via industriella bussar, och presenterar telemetrin live på skärmen samt via en skyddad webbserver.

---

## ✨ Huvudfunktioner

* **☀️ Universell Victron BLE-mottagning:** Fullt stöd för *Instant Readout* från SmartSolar MPPT, SmartShunt, BMV och Inverters. Hanterar hårdvaruaccelererad **AES-128-CTR dekryptering** via enhetens unika *Bindkey*.
* **🏷️ RuuviTag Integration:** Automatisk igenkänning och avkodning av miljödata (Temperatur, Luftfuktighet) från RuuviTags (Dataformat 5).
* **🔌 Trådade bussar (RS485 & CAN):** Integrerad drivrutin för Modbus RTU via hårdvaru-UART samt CAN-buss via ESP32:s inbyggda TWAI-kontroller.
* **⏰ Avancerad Klocka & Klocklopp:** NTP-synkroniserad realtidsklocka som automatiskt backas upp till kortets hårdvaru-RTC (**PCF85063**). Fungerar helt offline tack vare integrerat CR1220-batteri.
* **📅 Schemaläggare (Events):** Tidsstyrda händelser för att slå på/av reläer över Modbus RTU med inbyggd **Relä-vakt** (blockerar events om relämodulen kopplas ur fysiskt från RS485-bussen).
* **🖥️ Intelligent Skärmhantering (LVGL v8):** 
  * Hamburgermeny med touchzoner för smidig navigering mellan 5 dedikerade undersidor.
  * Startskärm med en snygg "CABBY v0.8"-toningsanimation.
  * **Automatiskt viloläge:** Skärmen lyser upp till 100% vid touch, men dämpar belysningen (PWM) till inställda nivåer för dag- respektive nattsänkning.
* **🔒 Säker Webbserver:** Session- och cookie-baserad adminpanel (ej Basic Auth) för fältkonfiguration, parning och namngivning av enheter.
* **🚨 Fältsäkerhet (Factory Reset):** Separerade nollställningsknappar (både på LCD och webb) för att rensa BLE-parningar eller återställa bussar till säkra standardvärden (9600bps/500Kbps) utan att tappa WiFi-konfigurationen.

---

## 🏗️ Hårdvaruspecifikation (Waveshare v4 Pinout)

Systemet är verifierat mot Waveshares förstapartsscheman:
* **I2C Sensorbuss (RTC / IO-Expander):** SDA = GPIO 11, SCL = GPIO 12
* **RS485 (UART1):** RX = GPIO 43, TX = GPIO 44 (Automatisk flödeskontroll)
* **CAN Bus (TWAI):** RX = GPIO 41, TX = GPIO 42
* **Bakgrundsbelysning (PWM):** GPIO 1 (LEDC Channel 0, 5KHz)
* **LiPo ADC Mätning:** GPIO 4 (Spänningsdelare, 12-bit)

---

## 📂 Projektstruktur

Skapa en mapp kallad `Victron_Touch_Server` och lägg till följande filer:
* `Victron_Touch_Server.ino` - Huvudloop och FreeRTOS Kärnseparering (Core 0/1).
* `config.h` / `config.cpp` - Globala datastrukturer, minneslås (Mutex) och NVS Flash-hantering.
* `bus_config.h` - Dedikerad, isolerad konfigurationsfil för RS485- och CAN-hastigheter.
* `clock_manager.h` - Logik för NTP, manuell tidssättning och I2C-hårdvaru-RTC.
* `industrial_busses.h` - Modbus-RTU, paketbyggnad med CRC16 samt TWAI/CAN-mottagning.
* `victron_ble.h` / `ruuvi_ble.h` - BLE-skannrar samt asynkron AES-dekrypteringsmotor.
* `web_server.h` - Cookie-autentisering och sektionsuppdelad, responsiv administratörspanel.
* `ui.h` / `ui.cpp` - LVGL v8-layout, touch-callbacks, sliders och ljusstyrkestyrning.

---

## 🛠️ Kompileringsinställningar (Arduino IDE 2.3.10)

För att systemet ska köras stabilt utan minnesbrist eller krascher måste följande inställningar väljas under fliken **Tools**:

1. **Board:** Välj `ESP32S3 Dev Module`
2. **PSRAM:** Välj `OPI PSRAM` *(Absolut nödvändigt för LVGL:s RGB-framebuffers)*
3. **Partition Scheme:** Välj `16MB Flash (3MB APP / 9MB FATFS)` eller `Huge APP`
4. **USB CDC On Boot:** Välj `Enabled` *(För att se Serial Monitor-utskrifterna direkt)*
5. **Core Debug Level:** Välj `None` eller `Error` *(Minskar overhead på seriebussen)*

---

## 📡 Serie-diagnostik (30-sekunders Watchdog)

För att maximera prestandan för touch-skärmen är Serial Monitor-utskrifterna strypta till att dumpa en samlad statusrapport var 30:e sekund:

```text
==================================================
[SYSTEM DIAGNOSTIK] 2026-09-02 14:15:00
==================================================
🔋 Hårdvara  | LiPo: 4.02 V (OK) | RTC-I2C: ONLINE
📡 Nätverk   | Inställda intervall -> Victron: 5s | Ruuvi: 15s
🔌 RS485     | Relä-vakt: AKTIV | Fysiskt relä: 🟢 ANSLUTET
--------------------------------------------------
☀️ Victron Solar status:
   -> [SmartSolar_MPPT] Spänning: 24.30 V | Effekt: 145 W | Sett: 14:14:58
   -> [SmartShunt_Main] Spänning: 13.12 V | Nivå: 98 %    | Sett: 14:14:55
🏷️ RuuviTags status:
   -> [Kylskåpet] Temp: 4.2 °C   | Sett: 14:14:50
   -> [Ute_Mätare] Temp: 19.1 °C | Sett: 14:14:45
==================================================
```

---

## 📄 Licens

Detta projekt är utvecklat för privat och industriell energiövervakning. Fritt att använda och modifiera under MIT-licensen.

###
AI -> https://share.google/aimode/HzsSuUqKbVzJT8Djm
