#pragma once
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "logger.h"

// Define these in your private config or use placeholders
#define FIREBASE_DATABASE_URL "https://tucapyenergy-default-rtdb.europe-west1.firebasedatabase.app"
#define API_KEY "AIzaSyAA9CCYeT44Mt8BLOkqpL4uu3cp5gpaevs"

namespace FirebaseHandler {

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
    config.database_url = FIREBASE_DATABASE_URL;
    config.api_key = API_KEY;
    
    // Povolit přístup bez přihlášení (pokud jsou pravidla v Firebase na "true")
    config.signer.test_mode = true;

    // Connect without auth
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Testovací zápis hned po startu, aby databáze nebyla "null"
    Firebase.RTDB.setString(&fbdo, "/system_logs", "ESP32 System Started - Firebase Connected");
}

void updateData(float battery_P, float battery_I, float grid_I, float battery_soc, String status_msg, String version) {
    FirebaseJson json;
    json.set("battery_P", battery_P);
    json.set("battery_I", battery_I);
    json.set("grid_I", grid_I);
    json.set("battery_soc", battery_soc);
    json.set("status_msg", status_msg);
    json.set("version", version);
    json.set("last_update", String(millis()));

    if (Firebase.RTDB.setJSON(&fbdo, "/energy_data", &json)) {
        // webLog("Firebase update OK");
    } else {
        webLog("Firebase error: " + fbdo.errorReason());
    }
}

void pushLog(String msg) {
    if (Firebase.RTDB.setString(&fbdo, "/system_logs", msg)) {
        // webLog("Firebase log OK");
    }
}

} // namespace FirebaseHandler
