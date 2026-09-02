#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"
#include "clock_manager.h"
#include "industrial_busses.h"

extern WebServer server;
extern String currentSessionToken;
extern unsigned long sessionTimeout;

inline bool isAuthorized() {
    if (millis() > sessionTimeout) { currentSessionToken = ""; return false; }
    if (server.hasHeader("Cookie")) {
        String cookie = server.header("Cookie");
        if (cookie.indexOf("CABBY_SESS=" + currentSessionToken) != -1) {
            sessionTimeout = millis() + 600000; // Förläng 10 min
            return true;
        }
    }
    return false;
}

inline void initWebServer() {
    WiFi.softAP("CABBY_Gateway_AP", "12345678");
    MDNS.begin("cabby");
    
    const char* headerkeys[] = {"Cookie"};
    server.collectHeaders(headerkeys, 1);

    server.on("/login", HTTP_GET, []() {
        String html; html.reserve(1024);
        html += "<html><head><meta charset='utf-8'><title>Cabby Login</title></head>";
        html += "<body style='font-family:sans-serif; text-align:center; padding-top:100px; background:#f4f4f9;'>";
        html += "<h2>🔒 Logga in på CABBY v0.8</h2><form action='/login' method='POST'>";
        html += "Anv: <input type='text' name='user'><br><br>Losen: <input type='password' name='pass'><br><br>";
        html += "<input type='submit' value='Logga in'></form></body></html>";
        server.send(200, "text/html", html);
    });

    server.on("/login", HTTP_POST, []() {
        if (server.arg("user") == "admin" && server.arg("pass") == "password123") {
            currentSessionToken = String(random(100000, 999999));
            sessionTimeout = millis() + 600000;
            server.sendHeader("Set-Cookie", "CABBY_SESS=" + currentSessionToken + "; Path=/; HttpOnly");
            server.sendHeader("Location", "/");
            server.send(302, "text/plain", "");
        } else {
            server.send(401, "text/html", "Fel uppgifter. <a href='/login'>Forsok igen</a>");
        }
    });

    server.on("/", []() {
        if (!isAuthorized()) { server.sendHeader("Location", "/login"); server.send(302, "text/plain", ""); return; }

        String html; html.reserve(4096);
        html += "<html><head><meta charset='utf-8'><title>Cabby Hub</title></head><body style='font-family:sans-serif; background:#f0f2f5; padding:20px;'>";
        html += "<h1>⚙️ CABBY v0.8 Kontrollpanel</h1><p>Tid: " + getFormattedTime() + " | Volt: " + String(readBatteryVoltage(),2) + "V</p>";
        
        // Sektion: Victron
        html += "<div style='background:white; padding:15px; margin-bottom:15px; border-radius:5px;'><h2>☀️ Victron BLE Enheter</h2>";
        html += "<form action='/save-v-int' method='POST'>Intervall: <input type='number' name='v_int' value='" + String(sysSettings.victronScanInterval) + "'>s <input type='submit' value='Spara'></form>";
        html += "<table>";
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            for(int i=0; i<victronCount; i++) {
                html += "<tr><td><b>" + String(savedVictrons[i].name) + "</b></td><td>" + String(savedVictrons[i].mac) + "</td><td>" + String(savedVictrons[i].batteryVoltage) + "V</td></tr>";
            }
            xSemaphoreGive(dataMutex);
        }
        html += "</table></div>";
        
        // Sektion: Bussar
        html += "<div style='background:white; padding:15px; margin-bottom:15px; border-radius:5px;'><h2>🔌 Busshastigheter</h2>";
        html += "<form action='/save-busses' method='POST'>RS485 Baud: <input type='number' name='baud' value='" + String(busSettings.rs485Baud) + "'><br>CAN Kbps: <input type='number' name='can' value='" + String(busSettings.canSpeedKbps) + "'><br><input type='submit' value='Spara'></form></div>";
        
        html += "</body></html>";
        server.send(200, "text/html", html);
    });

    server.on("/save-v-int", HTTP_POST, []() {
        if (!isAuthorized()) return;
        sysSettings.victronScanInterval = server.arg("v_int").toInt();
        saveAllSettings();
        server.sendHeader("Location", "/"); server.send(302, "text/plain", "");
    });

    server.on("/save-busses", HTTP_POST, []() {
        if (!isAuthorized()) return;
        busSettings.rs485Baud = server.arg("baud").toInt();
        busSettings.canSpeedKbps = server.arg("can").toInt();
        saveBusSettings();
        initRS485();
        server.sendHeader("Location", "/"); server.send(302, "text/plain", "");
    });

    server.begin();
}

inline void handleWebServer() { server.handleClient(); }

#endif
