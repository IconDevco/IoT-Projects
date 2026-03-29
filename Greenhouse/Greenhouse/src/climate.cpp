// climate.cpp
#include "climate.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ------------------------------------------------------------
// FETCH CLIMATE RSS / JSON
// ------------------------------------------------------------
bool fetchClimateRSS(const SystemConfig &config, ClimateData &out) {
    Serial.println("Fetching climate RSS...");

    HTTPClient http;
    http.begin(config.climate.rss_url);

    int code = http.GET();
    if (code != 200) {
        Serial.printf("Climate RSS error: %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        Serial.printf("Climate JSON error: %s\n", err.c_str());
        return false;
    }

    // These keys depend on your RSS → JSON converter
    out.temp_f     = doc["temp_f"]     | 75;
    out.humidity   = doc["humidity"]   | 40;
    out.uv_index   = doc["uv_index"]   | 5;
    out.cloud_cover= doc["cloud_cover"]| 20;

    Serial.printf("Climate: %dF, %d%% humidity, UV %d, clouds %d%%\n",
                  out.temp_f, out.humidity, out.uv_index, out.cloud_cover);

    return true;
}

// ------------------------------------------------------------
// APPLY CLIMATE MULTIPLIERS TO PUMP SCHEDULE
// ------------------------------------------------------------
void applyClimateAdjustments(SystemConfig &config, const ClimateData &climate) {
    float mult = 1.0;

    if (climate.temp_f >= config.climate.hot_threshold_f) {
        mult = config.climate.adjustments.hot_multiplier;
        Serial.println("Climate: HOT multiplier applied.");
    }
    else if (climate.temp_f >= config.climate.warm_threshold_f) {
        mult = config.climate.adjustments.warm_multiplier;
        Serial.println("Climate: WARM multiplier applied.");
    }
    else if (climate.temp_f <= config.climate.cold_threshold_f) {
        mult = config.climate.adjustments.cold_multiplier;
        Serial.println("Climate: COLD multiplier applied.");
    }
    else if (climate.temp_f <= config.climate.cool_threshold_f) {
        mult = config.climate.adjustments.cool_multiplier;
        Serial.println("Climate: COOL multiplier applied.");
    }

    // Apply multiplier to each pump
    for (auto &p : config.pumps) {
        p.run_minutes     = round(p.run_minutes * mult);
        p.interval_minutes= round(p.interval_minutes / mult);

        if (p.run_minutes < 1) p.run_minutes = 1;
        if (p.interval_minutes < 5) p.interval_minutes = 5;
    }
}