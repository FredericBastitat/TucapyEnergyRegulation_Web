#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

#define VERSION_URL  "https://raw.githubusercontent.com/FredericBastitat/TucapyEnergyRegulation/main/version.txt"
#define FIRMWARE_URL "https://raw.githubusercontent.com/FredericBastitat/TucapyEnergyRegulation/main/firmware.bin"

Preferences prefs;

String getCurrentSHA() {
    prefs.begin("ota", true);
    String sha = prefs.getString("sha", "");
    prefs.end();
    return sha;
}

void saveSHA(String sha) {
    prefs.begin("ota", false);
    prefs.putString("sha", sha);
    prefs.end();
}

String fetchString(String url) {
    HTTPClient http;
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    String result = "";
    if (http.GET() == 200) {
        result = http.getString();
        result.trim();
    }
    http.end();
    return result;
}

void checkAndUpdate() {
    Serial.println("Kontroluji verzi...newversion");
    String latestSHA = fetchString(VERSION_URL);
    if (latestSHA.isEmpty()) {
        Serial.println("Nepodarilo se ziskat verzi.");
        return;
    }

    String currentSHA = getCurrentSHA();
    Serial.println("Ulozena SHA:  " + currentSHA);
    Serial.println("GitHub SHA:   " + latestSHA);

    if (latestSHA == currentSHA) {
        Serial.println("Firmware je aktualni.");
        return;
    }

    Serial.println("Novy firmware nalezen, stahuji...");
    HTTPClient http;
    http.begin(FIRMWARE_URL);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (http.GET() == 200) {
        int size = http.getSize();
        WiFiClient* stream = http.getStreamPtr();

        if (Update.begin(size)) {
            Update.writeStream(*stream);
            if (Update.end(true)) {
                saveSHA(latestSHA);
                Serial.println("Update OK, restartuji...");
                ESP.restart();
            }
        }
    }
    http.end();
}

void setup() {
    Serial.begin(115200);
    WiFi.begin("DvorNet", "dvor62tuc");
    while (WiFi.status() != WL_CONNECTED) delay(500);
    checkAndUpdate();
}

void loop() {
    // každých 60 sekund zkontroluj
    checkAndUpdate();
    delay(2000);
}
