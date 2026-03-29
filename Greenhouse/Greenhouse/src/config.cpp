// config.cpp
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

bool loadConfig(SystemConfig &config) {
    if (!LittleFS.exists("/config.json")) {
        Serial.println("Config file missing.");
        return false;
    }

    File file = LittleFS.open("/config.json", "r");
    if (!file) {
        Serial.println("Failed to open config.json");
        return false;
    }

    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.printf("JSON parse error: %s\n", err.c_str());
        return false;
    }

    // WiFi
    config.wifi_ssid = doc["wifi"]["ssid"].as<String>();
    config.wifi_password = doc["wifi"]["password"].as<String>();

    // MQTT
    config.mqtt.broker = doc["mqtt"]["broker"].as<String>();
    config.mqtt.port = doc["mqtt"]["port"].as<int>();
    config.mqtt.client_id = doc["mqtt"]["client_id"].as<String>();
    config.mqtt.username = doc["mqtt"]["username"].as<String>();
    config.mqtt.password = doc["mqtt"]["password"].as<String>();
    config.mqtt.topic_prefix = doc["mqtt"]["topic_prefix"].as<String>();

    // Location
    config.location.latitude = doc["location"]["latitude"].as<float>();
    config.location.longitude = doc["location"]["longitude"].as<float>();
    config.location.timezone_offset = doc["location"]["timezone_offset"].as<int>();

    // Climate
    config.climate.rss_url = doc["climate"]["rss_url"].as<String>();
    config.climate.hot_threshold_f = doc["climate"]["hot_threshold_f"].as<int>();
    config.climate.warm_threshold_f = doc["climate"]["warm_threshold_f"].as<int>();
    config.climate.cool_threshold_f = doc["climate"]["cool_threshold_f"].as<int>();
    config.climate.cold_threshold_f = doc["climate"]["cold_threshold_f"].as<int>();

    config.climate.adjustments.hot_multiplier = doc["climate"]["adjustments"]["hot_multiplier"].as<float>();
    config.climate.adjustments.warm_multiplier = doc["climate"]["adjustments"]["warm_multiplier"].as<float>();
    config.climate.adjustments.cool_multiplier = doc["climate"]["adjustments"]["cool_multiplier"].as<float>();
    config.climate.adjustments.cold_multiplier = doc["climate"]["adjustments"]["cold_multiplier"].as<float>();

    // Pumps
    config.pumps.clear();
    JsonArray arr = doc["pumps"].as<JsonArray>();
    for (JsonObject p : arr) {
        PumpConfig pc;
        pc.id = p["id"].as<int>();
        pc.pin = p["pin"].as<int>();
        pc.enabled = p["enabled"].as<bool>();
        pc.run_minutes = p["run_minutes"].as<int>();
        pc.interval_minutes = p["interval_minutes"].as<int>();
        pc.sunrise_boost_minutes = p["sunrise_boost_minutes"].as<int>();
        pc.sunset_boost_minutes = p["sunset_boost_minutes"].as<int>();
        config.pumps.push_back(pc);
    }

    // Safe Mode
    config.safe_mode.fallback_run_minutes = doc["safe_mode"]["fallback_run_minutes"].as<int>();
    config.safe_mode.fallback_interval_minutes = doc["safe_mode"]["fallback_interval_minutes"].as<int>();
    config.safe_mode.deep_sleep_minutes = doc["safe_mode"]["deep_sleep_minutes"].as<int>();

    Serial.println("Config loaded successfully.");
    return true;
}

bool saveConfig(const SystemConfig &config) {
    StaticJsonDocument<4096> doc;

    // WiFi
    doc["wifi"]["ssid"] = config.wifi_ssid;
    doc["wifi"]["password"] = config.wifi_password;

    // MQTT
    doc["mqtt"]["broker"] = config.mqtt.broker;
    doc["mqtt"]["port"] = config.mqtt.port;
    doc["mqtt"]["client_id"] = config.mqtt.client_id;
    doc["mqtt"]["username"] = config.mqtt.username;
    doc["mqtt"]["password"] = config.mqtt.password;
    doc["mqtt"]["topic_prefix"] = config.mqtt.topic_prefix;

    // Location
    doc["location"]["latitude"] = config.location.latitude;
    doc["location"]["longitude"] = config.location.longitude;
    doc["location"]["timezone_offset"] = config.location.timezone_offset;

    // Climate
    doc["climate"]["rss_url"] = config.climate.rss_url;
    doc["climate"]["hot_threshold_f"] = config.climate.hot_threshold_f;
    doc["climate"]["warm_threshold_f"] = config.climate.warm_threshold_f;
    doc["climate"]["cool_threshold_f"] = config.climate.cool_threshold_f;
    doc["climate"]["cold_threshold_f"] = config.climate.cold_threshold_f;

    doc["climate"]["adjustments"]["hot_multiplier"] = config.climate.adjustments.hot_multiplier;
    doc["climate"]["adjustments"]["warm_multiplier"] = config.climate.adjustments.warm_multiplier;
    doc["climate"]["adjustments"]["cool_multiplier"] = config.climate.adjustments.cool_multiplier;
    doc["climate"]["adjustments"]["cold_multiplier"] = config.climate.adjustments.cold_multiplier;

    // Pumps
    JsonArray arr = doc.createNestedArray("pumps");
    for (auto &p : config.pumps) {
        JsonObject o = arr.createNestedObject();
        o["id"] = p.id;
        o["pin"] = p.pin;
        o["enabled"] = p.enabled;
        o["run_minutes"] = p.run_minutes;
        o["interval_minutes"] = p.interval_minutes;
        o["sunrise_boost_minutes"] = p.sunrise_boost_minutes;
        o["sunset_boost_minutes"] = p.sunset_boost_minutes;
    }

    // Safe Mode
    doc["safe_mode"]["fallback_run_minutes"] = config.safe_mode.fallback_run_minutes;
    doc["safe_mode"]["fallback_interval_minutes"] = config.safe_mode.fallback_interval_minutes;
    doc["safe_mode"]["deep_sleep_minutes"] = config.safe_mode.deep_sleep_minutes;

    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        Serial.println("Failed to write config.json");
        return false;
    }

    serializeJsonPretty(doc, file);
    file.close();

    Serial.println("Config saved.");
    return true;
}