#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ModbusMaster.h>
#include "ota.h"
#include "logger.h"

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
WebSocketsServer webSocket(81);
WebServer server(80);
String logBuffer = "";

void preTransmission() { digitalWrite(DE_RE_PIN, HIGH); }
void postTransmission() { digitalWrite(DE_RE_PIN, LOW); }

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_CONNECTED) {
        webSocket.sendTXT(num, logBuffer);
    }
}

void handleRoot() {
  String html = R"raw(
<!DOCTYPE html>
<html lang='cs'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Energy Dashboard & Console</title>
    <style>
        :root {
            --bg: #0f172a;
            --card: rgba(30, 41, 59, 0.7);
            --primary: #38bdf8;
            --text: #f8fafc;
            --accent: #fbbf24;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: radial-gradient(circle at top right, #1e293b, #0f172a);
            color: var(--text);
            margin: 0;
            padding: 20px;
            display: flex;
            flex-direction: column;
            align-items: center;
            min-height: 100vh;
        }
        .container {
            width: 100%;
            max-width: 900px;
            gap: 20px;
            display: flex;
            flex-direction: column;
        }
        .dashboard {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            width: 100%;
        }
        .card {
            background: var(--card);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.1);
            border-radius: 16px;
            padding: 20px;
            text-align: center;
            box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
            transition: transform 0.2s;
        }
        .card:hover { transform: translateY(-5px); }
        .card h3 { margin: 0; font-size: 14px; color: var(--primary); text-transform: uppercase; }
        .card .value { font-size: 32px; font-weight: bold; margin: 10px 0; }
        .card .unit { font-size: 16px; opacity: 0.7; }
        
        .console-container {
            background: #000;
            border-radius: 12px;
            padding: 15px;
            box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.2);
            border: 1px solid #334155;
            height: 300px;
            display: flex;
            flex-direction: column;
        }
        .console-header {
            display: flex;
            justify-content: space-between;
            margin-bottom: 10px;
            font-size: 12px;
            color: #64748b;
            border-bottom: 1px solid #1e293b;
            padding-bottom: 5px;
        }
        #console {
            flex-grow: 1;
            overflow-y: auto;
            font-family: 'Courier New', Courier, monospace;
            font-size: 13px;
            line-height: 1.5;
            color: #10b981;
            white-space: pre-wrap;
        }
        .status-bar {
            width: 100%;
            text-align: center;
            font-size: 14px;
            color: var(--accent);
            padding: 10px;
        }
    </style>
</head>
<body>
    <div class='container'>
        <h1>Tucapy Energy</h1>
        <div class='dashboard'>
            <div class='card'><h3>Battery Power</h3><div class='value'>)raw" + String(battery_P) + R"raw(</div><span class='unit'>kW</span></div>
            <div class='card'><h3>Battery Current</h3><div class='value'>)raw" + String(battery_I) + R"raw(</div><span class='unit'>A</span></div>
            <div class='card'><h3>Grid Current</h3><div class='value'>)raw" + String(grid_I) + R"raw(</div><span class='unit'>A</span></div>
            <div class='card'><h3>SOC</h3><div class='value'>)raw" + String(battery_soc) + R"raw(</div><span class='unit'>%</span></div>
        </div>
        
        <div class='status-bar'>)raw" + status_msg + R"raw(</div>

        <div class='console-container'>
            <div class='console-header'>
                <span>WEBSOCKET CONSOLE</span>
                <span id='ws-status'>Connecting...</span>
            </div>
            <div id='console'></div>
        </div>
    </div>

    <script>
        const consoleEl = document.getElementById('console');
        const statusEl = document.getElementById('ws-status');
        let socket = new WebSocket('ws://' + window.location.hostname + ':81/');

        socket.onopen = () => {
            statusEl.innerText = 'ONLINE';
            statusEl.style.color = '#10b981';
        };

        socket.onclose = () => {
            statusEl.innerText = 'OFFLINE';
            statusEl.style.color = '#ef4444';
        };

        socket.onmessage = (event) => {
            consoleEl.innerText += event.data + '\n';
            consoleEl.scrollTop = consoleEl.scrollHeight;
        };

        // Auto-refresh data (simple way without complicating backend too much)
        setTimeout(() => location.reload(), 10000);
    </script>
</body>
</html>
)raw";
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
    webLog("Pripojuji WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    webLog("WiFi OK: " + WiFi.localIP().toString());

    server.on("/", handleRoot);
    server.begin();

    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);

    webLog("System start.");
    OTA::check();
}

void loop() {
    OTA::check();
    server.handleClient();
    webSocket.loop();

    static unsigned long lastRead = 0;
    if (millis() - lastRead > 3000) {
        lastRead = millis();
        webLog("Ctu data z menice...");
        
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
            webLog("Data prectena: SOC=" + String(battery_soc) + "%");
        } else {
            status_msg = "Chyba komunikace: 0x" + String(res, HEX);
            webLog("Chyba Modbus: 0x" + String(res, HEX));
        }
    }
}