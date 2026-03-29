#include <Arduino.h>
#include <LittleFS.h>

#include "config.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "climate.h"
#include "scheduler.h"
#include "pumps.h"
#include "mqtt_client.h"
#include "safe_mode.h"
#include "deep_sleep.h"

// ------------------------------------------------------------
// GLOBALS
// ------------------------------------------------------------
SystemConfig CONFIG;        // Loaded from LittleFS
bool safeModeActive = false;

// ------------------------------------------------------------
// SETUP
// ------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("\n--- ESP32-C3 Hydro Controller Booting ---");

    // --------------------------------------------------------
    // 1. Mount LittleFS
    // --------------------------------------------------------
    if (!LittleFS.begin()) {
        Serial.println("ERROR: LittleFS mount failed. Entering safe mode.");
        enterSafeMode(CONFIG);
    }

    // --------------------------------------------------------
    // 2. Load config.json
    // --------------------------------------------------------
    if (!loadConfig(CONFIG)) {
        Serial.println("ERROR: Config load failed. Entering safe mode.");
        enterSafeMode(CONFIG);
    }

    // --------------------------------------------------------
    // 3. Determine boot reason
    // --------------------------------------------------------
    BootReason reason = getBootReason();
    Serial.printf("Boot reason: %s\n", bootReasonToString(reason));

    // --------------------------------------------------------
    // 4. Connect Wi-Fi
    // --------------------------------------------------------
    if (!wifiConnect(CONFIG)) {
        Serial.println("WiFi failed. Entering safe mode.");
        enterSafeMode(CONFIG);
    }

    // --------------------------------------------------------
    // 5. Sync NTP time
    // --------------------------------------------------------
    if (!syncTime()) {
        Serial.println("NTP failed. Entering safe mode.");
        enterSafeMode(CONFIG);
    }

    // --------------------------------------------------------
    // 6. Fetch sunrise/sunset
    // --------------------------------------------------------
    if (!fetchSunTimes(CONFIG)) {
        Serial.println("Sunrise/sunset fetch failed. Entering safe mode.");
        enterSafeMode(CONFIG);
    }

    // --------------------------------------------------------
    // 7. Fetch climate RSS
    // --------------------------------------------------------
    ClimateData climate;
    if (!fetchClimateRSS(CONFIG, climate)) {
        Serial.println("Climate RSS failed. Entering safe mode.");
        enterSafeMode(CONFIG);
    }

    // --------------------------------------------------------
    // 8. Apply climate adjustments to pump schedules
    // --------------------------------------------------------
    applyClimateAdjustments(CONFIG, climate);

    // --------------------------------------------------------
    // 9. Initialize MQTT
    // --------------------------------------------------------
    if (!mqttInit(CONFIG)) {
        Serial.println("MQTT init failed. Entering safe mode.");
        enterSafeMode(CONFIG);
    }

    // --------------------------------------------------------
    // 10. Publish sunrise packet (if morning)
    // --------------------------------------------------------
    if (isMorning()) {
        mqttPublishSunrise(CONFIG, climate);
    }

    // --------------------------------------------------------
    // 11. Run pump event if due
    // --------------------------------------------------------
    PumpEvent evt;
    if (schedulerCheckEvent(CONFIG, evt)) {
        runPumpEvent(evt, CONFIG);
    }

    // --------------------------------------------------------
    // 12. Publish heartbeat
    // --------------------------------------------------------
    mqttPublishHeartbeat();

    // --------------------------------------------------------
    // 13. Check for config updates (non-blocking)
    // --------------------------------------------------------
    mqttProcessUpdates(CONFIG);

    // --------------------------------------------------------
    // 14. If sunset → publish daily summary
    // --------------------------------------------------------
    if (isSunset()) {
        mqttPublishSunset(CONFIG);
        resetDailyPumpCounters();
    }

    // --------------------------------------------------------
    // 15. Determine next wake time
    // --------------------------------------------------------
    uint64_t sleepSeconds = schedulerNextWake(CONFIG);
    Serial.printf("Next wake in %llu seconds\n", sleepSeconds);

    // --------------------------------------------------------
    // 16. Enter deep sleep
    // --------------------------------------------------------
    enterDeepSleep(sleepSeconds);
}

// ------------------------------------------------------------
// LOOP (unused)
// ------------------------------------------------------------
void loop() {
    // Empty — all logic runs in setup() then deep sleep
}