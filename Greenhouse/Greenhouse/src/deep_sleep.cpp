// deep_sleep.cpp
#include "deep_sleep.h"
#include <esp_sleep.h>

void enterDeepSleep(uint64_t seconds) {
    Serial.println("\n--- ENTERING DEEP SLEEP ---");

    uint64_t us = seconds * 1000000ULL;

    Serial.printf("Sleep duration: %llu seconds (%llu us)\n", seconds, us);

    // Configure wake timer
    esp_sleep_enable_timer_wakeup(us);

    Serial.println("Goodnight.");
    Serial.flush();

    // Enter deep sleep
    esp_deep_sleep_start();

    // Should never reach here
    while (true) {
        delay(1000);
    }
}