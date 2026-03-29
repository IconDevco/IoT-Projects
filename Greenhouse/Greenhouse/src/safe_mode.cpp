// safe_mode.cpp
#include "safe_mode.h"
#include "pumps.h"
#include "deep_sleep.h"
#include <time.h>

void enterSafeMode(const SystemConfig &config) {
    Serial.println("\n--- ENTERING SAFE MODE ---");

    int runMin = config.safe_mode.fallback_run_minutes;
    int sleepMin = config.safe_mode.deep_sleep_minutes;

    Serial.printf("Safe mode: run %d min, sleep %d min\n",
                  runMin, sleepMin);

    // Run all enabled pumps for fallback duration
    for (auto &p : config.pumps) {
        if (!p.enabled) continue;

        Serial.printf("Safe mode: running pump %d\n", p.id);

        pumpOn(p.pin);

        uint32_t duration = runMin * 60UL * 1000UL;
        uint32_t start = millis();

        while (millis() - start < duration) {
            delay(100);
        }

        pumpOff(p.pin);
    }

    Serial.println("Safe mode pump cycle complete.");

    // Deep sleep
    uint64_t sleepSeconds = sleepMin * 60ULL;
    Serial.printf("Safe mode: sleeping %llu seconds\n", sleepSeconds);

    enterDeepSleep(sleepSeconds);

    // Should never reach here
    while (true) delay(1000);
}