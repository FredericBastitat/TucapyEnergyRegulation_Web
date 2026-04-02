#pragma once
#include <Arduino.h>
#include <time.h>

extern String logBuffer;
#define MAX_LOG_SIZE 2048

class Time
{
    public:
        Time()
        {
            //Wifi....

            //Time
            configTime(3600,3600,"pool.ntp.org","time.google.com");
            tm time;
            while (!getLocalTime(&time))
            {
            Serial.println("Čekám na čas...");
            delay(500);
            }
            Serial.println("Čas synchronizován!");
        }

        bool update() {
        return getLocalTime(&time);
        }

        int hour()const{return time.tm_hour;}
        int day()const{return time.tm_mday;}
        int min()const{return time.tm_min;}
        int sec()const{return time.tm_sec;}

        String convert()
        {
            char buf[9];
            sprintf(buf, "%02d:%02d:%02d", time.tm_hour, time.tm_min,time.tm_sec);
            return String(buf);
        }

    private:
    tm time = {};
};





inline void webLog(String msg, bool toSerial = false) {
    Time t;
    String logEntry = "[" + t.convert() + "s] " + msg;
    if (toSerial) {
        Serial.println(logEntry);
    }
    
    // Add to buffer (bude odeslán přes Firebase)
    logBuffer += logEntry + "\n";
    if (logBuffer.length() > MAX_LOG_SIZE) {
        logBuffer = logBuffer.substring(logBuffer.length() - MAX_LOG_SIZE);
    }
}


