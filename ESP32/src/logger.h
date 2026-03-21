#pragma once
#include <Arduino.h>
#include <WebSocketsServer.h>

extern WebSocketsServer webSocket;
extern String logBuffer;
#define MAX_LOG_SIZE 2048

inline void webLog(String msg, bool toSerial = false) {
    String logEntry = "[" + String(millis()/1000) + "s] " + msg;
    if (toSerial) {
        Serial.println(logEntry);
    }
    
    // Add to buffer
    logBuffer += logEntry + "\n";
    if (logBuffer.length() > MAX_LOG_SIZE) {
        logBuffer = logBuffer.substring(logBuffer.length() - MAX_LOG_SIZE);
    }
    
    // Send to websocket
    webSocket.broadcastTXT(logEntry);
}
