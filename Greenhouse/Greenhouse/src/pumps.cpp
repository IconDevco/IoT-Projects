// pumps.cpp
#include "pumps.h"
#include <time.h>

// ------------------------------------------------------------
// Initialize pump GPIO pins
// ------------------------------------------------------------
void pumpsInit(const SystemConfig &config) {
    for (auto &p : config.pumps) {
        pinMode(p.pin, OUTPUT);
        digitalWrite(p.pin, LOW); // pumps OFF by default
    }
}

// ------------------------------------------------------------
// Turn pump ON
// ------------------------------------------------------------
void pumpOn(int pin) {
    Serial.printf("Pump ON (pin %d)\n", pin);
    digitalWrite(pin, HIGH);
}

// ------------------------------------------------------------
// Turn pump OFF
// ------------------------------------------------------------
void pumpOff(int pin) {
    Serial.printf("Pump OFF (pin %d)\n", pin);
    digitalWrite(pin, LOW);
}

// ------------------------------------------------------------
// Run a pump event for X minutes
// ------------------------------------------------------------
void runPumpEvent(const PumpEvent &evt, const SystemConfig &config) {
    Serial.printf("Running pump %d for %d minutes...\n",
                  evt.pump_id, evt.run_minutes);

    // Find pump pin
    int pin = -1;
    for (auto &p : config.pumps) {
        if (p.id == evt.pump_id) {
            pin = p.pin;
            break;
        }
    }

    if (pin < 0) {
        Serial.println("ERROR: Pump ID not found.");
        return;
    }

    pumpOn(pin);

    // Convert minutes → milliseconds
    uint32_t duration = evt.run_minutes * 60UL * 1000UL;

    uint32_t start = millis();
    while (millis() - start < duration) {
        delay(100); // keep loop responsive
    }

    pumpOff(pin);

    Serial.println("Pump event complete.");
}