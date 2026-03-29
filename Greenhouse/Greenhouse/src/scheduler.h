// scheduler.h
#pragma once
#include <Arduino.h>
#include "config.h"

// A scheduled pump event
struct PumpEvent {
    int pump_id;
    int run_minutes;
};

// Track last run times (in epoch seconds)
struct PumpState {
    int pump_id;
    uint32_t last_run_epoch;
    int minutes_today;
};

bool schedulerInit(const SystemConfig &config);
bool schedulerCheckEvent(const SystemConfig &config, PumpEvent &evt);
uint64_t schedulerNextWake(const SystemConfig &config);
void resetDailyPumpCounters();