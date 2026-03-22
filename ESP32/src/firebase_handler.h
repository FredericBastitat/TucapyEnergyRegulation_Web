#pragma once
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "logger.h"

#define FIREBASE_DATABASE_URL "tucapyenergy-default-rtdb.europe-west1.firebasedatabase.app"
#define API_KEY "AIzaSyAA9CCYeT44Mt8BLOkqpL4uu3cp5gpaevs"

namespace FirebaseHandler {

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseReady = false;

void setup() {
    config.database_url = FIREBASE_DATABASE_URL;
    config.api_key = API_KEY;
    
    // Test mode – ESP32 nemá přihlašovací údaje, takže musí být true
    config.signer.test_mode = true;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Počkáme krátce na inicializaci
    unsigned long t = millis();
    while (!Firebase.ready() && millis() - t < 5000) {
        delay(100);
    }

    firebaseReady = Firebase.ready();
    if (firebaseReady) {
        Serial.println("Firebase: READY");
    } else {
        Serial.println("Firebase: NOT READY after 5s");
    }
}

void updateData(float battery_P, float battery_I, float grid_I, float battery_soc, String status_msg, String version) {
    if (!Firebase.ready()) {
        static unsigned long lastWarn = 0;
        if (millis() - lastWarn > 10000) {
            webLog("Firebase NOT READY", true);
            lastWarn = millis();
        }
        return;
    }

    FirebaseJson json;
    json.set("battery_P", battery_P);
    json.set("battery_I", battery_I);
    json.set("grid_I", grid_I);
    json.set("battery_soc", battery_soc);
    json.set("status_msg", status_msg);
    json.set("version", version);
    json.set("last_update", (int)(millis() / 1000));

    if (!Firebase.RTDB.setJSON(&fbdo, "/energy_data", &json)) {
        webLog("FB err(" + String(fbdo.httpCode()) + "): " + fbdo.errorReason());
    }
}

// Odesílá konzolové logy do Firebase pro zobrazení ve Vercel app
void pushConsoleLogs(const String& logBuffer) {
    if (!Firebase.ready()) return;
    
    // Uložíme posledních max 2000 znaků logu
    String trimmed = logBuffer;
    if (trimmed.length() > 2000) {
        trimmed = trimmed.substring(trimmed.length() - 2000);
    }

    if (!Firebase.RTDB.setString(&fbdo, "/console_logs", trimmed)) {
        // Tiše ignorujeme chybu konzolových logů
    }
}

} // namespace FirebaseHandler
