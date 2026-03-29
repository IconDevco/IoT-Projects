// config.h
#pragma once
#include <Arduino.h>
#include <vector>

// -----------------------------
// Pump Configuration
// -----------------------------
struct PumpConfig {
    int id;
    int pin;
    bool enabled;
    int run_minutes;
    int interval_minutes;
    int sunrise_boost_minutes;
    int sunset_boost_minutes;
};

// -----------------------------
// Climate Configuration
// -----------------------------
struct ClimateAdjustments {
    float hot_multiplier;
    float warm_multiplier;
    float cool_multiplier;
    float cold_multiplier;
};

struct ClimateConfig {
    String rss_url;
    int hot_threshold_f;
    int warm_threshold_f;
    int cool_threshold_f;
    int cold_threshold_f;
    ClimateAdjustments adjustments;
};

// -----------------------------
// MQTT Configuration
// -----------------------------
struct MQTTConfig {
    String broker;
    int port;
    String client_id;
    String username;
    String password;
    String topic_prefix;
};

// -----------------------------
// Location Configuration
// -----------------------------
struct LocationConfig {
    float latitude;
    float longitude;
    int timezone_offset;
};

// -----------------------------
// Safe Mode Configuration
// -----------------------------
struct SafeModeConfig {
    int fallback_run_minutes;
    int fallback_interval_minutes;
    int deep_sleep_minutes;
};

// -----------------------------
// System Configuration
// -----------------------------
struct SystemConfig {
    String wifi_ssid;
    String wifi_password;

    MQTTConfig mqtt;
    LocationConfig location;
    ClimateConfig climate;
    std::vector<PumpConfig> pumps;
    SafeModeConfig safe_mode;
};

// -----------------------------
// Function Prototypes
// -----------------------------
bool loadConfig(SystemConfig &config);
bool saveConfig(const SystemConfig &config);