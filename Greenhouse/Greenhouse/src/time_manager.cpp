// time_manager.cpp
#include "time_manager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_sleep.h>

SunTimes SUN;

// ------------------------------------------------------------
// NTP TIME SYNC
// ------------------------------------------------------------
bool syncTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print("Syncing NTP time");

    for (int i = 0; i < 20; i++) {
        time_t now = time(nullptr);
        if (now > 100000) {
            Serial.println("\nNTP time synced.");
            return true;
        }
        Serial.print(".");
        delay(500);
    }

    Serial.println("\nNTP sync failed.");
    return false;
}

// ------------------------------------------------------------
// FETCH SUNRISE / SUNSET
// ------------------------------------------------------------
bool fetchSunTimes(const SystemConfig &config) {
    HTTPClient http;

    String url = "https://api.sunrise-sunset.org/json?lat=" +
                 String(config.location.latitude, 6) +
                 "&lng=" +
                 String(config.location.longitude, 6) +
                 "&formatted=0";

    Serial.println("Fetching sunrise/sunset:");
    Serial.println(url);

    http.begin(url);
    int code = http.GET();

    if (code != 200) {
        Serial.printf("Sun API error: %d\n", code);
        http.end();
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();

    if (err) {
        Serial.printf("Sun JSON error: %s\n", err.c_str());
        return false;
    }

    String sunriseUTC = doc["results"]["sunrise"].as<String>();
    String sunsetUTC  = doc["results"]["sunset"].as<String>();

    // Store raw strings
    SUN.sunrise = sunriseUTC;
    SUN.sunset  = sunsetUTC;

    Serial.printf("Sunrise: %s\n", SUN.sunrise.c_str());
    Serial.printf("Sunset:  %s\n", SUN.sunset.c_str());

    return true;
}

// ------------------------------------------------------------
// MORNING / SUNSET CHECKS
// ------------------------------------------------------------
bool isMorning() {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    return (t->tm_hour >= 5 && t->tm_hour <= 10);
}

bool isSunset() {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    return (t->tm_hour >= 17 && t->tm_hour <= 20);
}

// ------------------------------------------------------------
// BOOT REASON
// ------------------------------------------------------------
BootReason getBootReason() {
    esp_reset_reason_t r = esp_reset_reason();

    switch (r) {
        case ESP_RST_POWERON:  return BOOT_POWER_ON;
        case ESP_RST_DEEPSLEEP:return BOOT_DEEP_SLEEP;
        case ESP_RST_BROWNOUT: return BOOT_BROWNOUT;
        case ESP_RST_WDT:      return BOOT_WATCHDOG;
        default:               return BOOT_UNKNOWN;
    }
}

const char* bootReasonToString(BootReason r) {
    switch (r) {
        case BOOT_POWER_ON:  return "power_on_reset";
        case BOOT_DEEP_SLEEP:return "deep_sleep_wake";
        case BOOT_BROWNOUT:  return "brownout";
        case BOOT_WATCHDOG:  return "watchdog";
        default:             return "unknown";
    }
}