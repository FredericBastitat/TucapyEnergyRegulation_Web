#include <Arduino.h>
#include <WiFi.h>
#include "ota.h"
#include "logger.h"
#include "modbus_handler.h"
#include "firebase_handler.h"

#define WIFI_SSID "DvorNet"
#define WIFI_PASS "dvor62tuc"
String currentVersion = "USB-Manual";

// --- OVLÁDÁNÍ VÝSTUPŮ ---
#define OUT1 23
#define OUT2 22
#define OUT3 21
#define OUT4 19
#define OUT5 32
#define OUT6 25
#define OUT7 26
#define OUT8 27
const int outputs[] = {OUT1, OUT2, OUT3, OUT4, OUT5, OUT6, OUT7, OUT8};
const int n_outputs=8;
#define HORNI_PROUD 2
#define SPODNI_PROUD 0  

#define HORNI_SOC 80
#define SPODNI_SOC 60

bool power_mode=false;
int idx=0;

String logBuffer = "";

void turn_on()
{
    if(idx<n_outputs)
    {
        digitalWrite(outputs[idx],LOW);
        idx++;
    }
    return;
}
void turn_off()
{

    if (idx>0)idx--;
    else return;
    digitalWrite(outputs[idx],HIGH);
    return;
}


void setup() {
    Serial.begin(115200);
    
    // Inicializace Modbus
    ModbusHandler::setup();
    
    // Inicializace výstupů
    for (int pin : outputs) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }
    
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    // Čekáme na WiFi
    int att = 0;
    while (WiFi.status() != WL_CONNECTED && att < 30) {
        delay(500);
        Serial.print(".");
        att++;
    }

    // Inicializace Firebase až po WiFi
    FirebaseHandler::setup();

    // Načteme SHA z paměti pro UI Dashboard
    currentVersion = OTA::getCurrentSHA();
    if (currentVersion == "") currentVersion = "Manual-USB";
    else currentVersion = currentVersion.substring(0, 7);

    webLog("Start systemu - verze " + currentVersion);
}

void loop() {
    static unsigned long lastOTA = millis();

    // OTA kontrola az po par minutach
    if (millis() > 60000 && (millis() - lastOTA > 60000)) {
        lastOTA = millis(); 
        OTA::check();
    }

    // Čtení dat z měniče
    bool modbusOK = ModbusHandler::update();
    
    if(modbusOK)
    {
        static unsigned long lastSwitch = 0;
        
        if(ModbusHandler::battery_soc<SPODNI_SOC)power_mode=false;
        if(ModbusHandler::battery_soc>=HORNI_SOC)power_mode=true;
        
        // Mezi přepnutím stupňů čekáme aspoň 10 vteřin pro ustálení
        if (millis() - lastSwitch > 10000) {
            if(power_mode && (ModbusHandler::battery_I>=HORNI_PROUD)) {
                turn_on();
                lastSwitch = millis();
            }
            if(power_mode && (ModbusHandler::battery_I<=SPODNI_PROUD)) {
                turn_off();
                lastSwitch = millis();
            }
        }
    }

    // Push to Firebase
    static unsigned long lastFirebasePush = 0;
    if (millis() - lastFirebasePush > 5000) {
        lastFirebasePush = millis();
        FirebaseHandler::updateData(
            ModbusHandler::battery_P,
            ModbusHandler::battery_I,
            ModbusHandler::grid_I,
            ModbusHandler::battery_soc,
            ModbusHandler::status_msg,
            currentVersion,
            logBuffer
        );
    }
}