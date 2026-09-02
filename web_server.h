#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"
#include "clock_manager.h"
#include "industrial_busses.h"

WebServer server(80);

inline bool isAuthorized() {
    if (millis() > sessionTimeout) { currentSessionToken = ""; return false; }
    if (server.hasHeader("Cookie")) {
        String cookie = server.header("Cookie");
        if (cookie.indexOf("CABBY_SESS=" + currentSessionToken) != -1) {
            sessionTimeout = millis() + 600000; 
            return true;
        }
    }
    return false;
}

inline void initWebServer() {
    const char* headerkeys[] = {"Cookie"};
    server.collectHeaders(headerkeys, 1);

    server.on("/login", HTTP_GET, []() {
        String html; html.reserve(1024);
        html += "<html><head><meta charset='utf-8'><title>Login</title></head>";
        html += "<body style='font-family:sans-serif; text-align:center; padding-top:100px; background:#f4f4f9;'>";
        html += "<h2>🔒 CABBY v0.8 Inloggning</h2><form action='/login' method='POST'>";
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
        html += "<html><head><meta charset='utf-8'><title>Cabby Control</title>";
        html += "<style>.card{background:white; padding:20px; margin-bottom:15px; border-radius:6px; box-shadow:0 2px 5px rgba(0,0,0,0.1);}</style></head>";
        html += "<body style='font-family:sans-serif; background:#f0f2f5; padding:20px;'>";
        html += "<h1>⚙️ CABBY v0.8 Huvudportal</h1><p>Tid: " + getFormattedTime() + " | Volt: " + String(readBatteryVoltage(),2) + "V</p>";
        
        // WiFi Sektion
        html += "<div class='card'><h2>📡 WiFi-Anslutning (Klientlage)</h2><form action='/save-wifi' method='POST'>";
        html += "Anslut till hemmanatverk: <input type='checkbox' name='usesta' value='1' " + String(sysSettings.useSTA ? "checked" : "") + "><br><br>";
        html += "SSID: <input type='text' name='ssid' value='" + String(sysSettings.wifiSSID) + "'><br>";
        html += "Losenord: <input type='password' name='pass' value='" + String(sysSettings.wifiPass) + "'><br><br>";
        html += "<input type='submit' value='Spara & Starta om'></form></div>";

        // Victron Sektion
        html += "<div class='card'><h2>☀️ Victron Enheter</h2><table>";
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            for(int i=0; i<victronCount; i++) {
                html += "<tr><td><b>" + String(savedVictrons[i].name) + "</b></td><td>" + String(savedVictrons[i].mac) + "</td><td>" + String(savedVictrons[i].batteryVoltage) + "V</td></tr>";
            }
            xSemaphoreGive(dataMutex);
        }
        html += "</table></div>";
        
        html += "</body></html>";
        server.send(200, "text/html", html);
    });

    server.on("/save-wifi", HTTP_POST, []() {
        if (!isAuthorized()) return;
        sysSettings.useSTA = server.hasArg("usesta");
        strncpy(sysSettings.wifiSSID, server.arg("ssid").c_str(), sizeof(sysSettings.wifiSSID) - 1);
        strncpy(sysSettings.wifiPass, server.arg("pass").c_str(), sizeof(sysSettings.wifiPass) - 1);
        saveAllSettings();
        server.send(200, "text/html", "Sparat! Startar om systemet...");
        delay(1000);
        ESP.restart();
    });

    server.begin();
}

inline void handleWebServer() { server.handleClient(); }

#endif
