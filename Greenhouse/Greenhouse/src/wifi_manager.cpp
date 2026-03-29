// wifi_manager.cpp
#include "wifi_manager.h"
#include <WiFi.h>

bool wifiConnect(const SystemConfig &config) {
    Serial.println("Connecting to WiFi...");
    Serial.printf("SSID: %s\n", config.wifi_ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());

    int attempts = 0;
    const int maxAttempts = 20;

    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("WiFi connection failed.");
    return false;
}