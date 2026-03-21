#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ModbusMaster.h>
#include "ota.h"

#define WIFI_SSID "DvorNet"
#define WIFI_PASS "dvor62tuc"



// Konfigurace RS485 (Serial0 - zakladni RX/TX piny)
#define DE_RE_PIN 18 

// Modbus Registry (GoodWe - FC 03)
#define SLAVE_ID 247
#define REG_BATTERY_POWER 30258
#define REG_BATTERY_I     30255
#define REG_SOC           33000
#define REG_GRID_I        11000

float battery_P = 0, battery_soc = 0, battery_I = 0, grid_I = 0;
String status_msg = "Cekam na data...";

ModbusMaster node;

void preTransmission() { digitalWrite(DE_RE_PIN, HIGH); }
void postTransmission() { digitalWrite(DE_RE_PIN, LOW); }

WebServer server(80);

void handleRoot() {
  String html = "<html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='5'></head>";
  html += "<body style='font-family:sans-serif; text-align:center; padding:50px;'>";
  html += "<h1>Inverter Dashboard</h1>";
  html += "<div style='font-size:24px; margin:20px; border:2px solid #333; padding:20px; display:inline-block;'>";
  html += "<b>Battery Power:</b> " + String(battery_P) + " kW<br>";
  html += "<b>Battery Current:</b> " + String(battery_I) + " A<br>";
  html += "<b>Grid Current:</b> " + String(grid_I) + " A<br>";
  html += "<b>SOC:</b> " + String(battery_soc) + " %";
  html += "</div>";
  html += "<p>Status: " + status_msg + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(9600);

    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);
  
    node.begin(SLAVE_ID, Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    delay(1000);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Pripojuji WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi OK: " + WiFi.localIP().toString());

    server.on("/", handleRoot);
    server.begin();

    OTA::check();
}

void loop() {
    OTA::check();
    server.handleClient();
      static unsigned long lastRead = 0;
  if (millis() - lastRead > 3000) {
    lastRead = millis();
    
    // 1. Cteni Battery Power (FC 03)
    uint8_t res = node.readHoldingRegisters(REG_BATTERY_POWER, 2);
    if (res == node.ku8MBSuccess) {
      battery_P = (int32_t)((node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1)) / 1000.0;
      
      // 2. Cteni SOC
      if (node.readHoldingRegisters(REG_SOC, 1) == node.ku8MBSuccess) {
        battery_soc = node.getResponseBuffer(0) / 100.0;
      }

      // 3. Cteni Battery Current
      if (node.readHoldingRegisters(REG_BATTERY_I, 1) == node.ku8MBSuccess) {
        battery_I = (int16_t)node.getResponseBuffer(0) / 10.0;
      }
      
      status_msg = "OK (Aktualizovano " + String(millis()/1000) + "s)";
    } else {
      status_msg = "Chyba komunikace: 0x" + String(res, HEX);
    }
  }



    delay(2000); // kontroluj každých 60s
}