// scheduler.cpp
#include "scheduler.h"
#include <vector>
#include <time.h>

static std::vector<PumpState> STATES;

// ------------------------------------------------------------
// Initialize pump states
// ------------------------------------------------------------
bool schedulerInit(const SystemConfig &config) {
    STATES.clear();

    for (auto &p : config.pumps) {
        PumpState s;
        s.pump_id = p.id;
        s.last_run_epoch = 0;
        s.minutes_today = 0;
        STATES.push_back(s);
    }

    return true;
}

// ------------------------------------------------------------
// Check if any pump should run on this wake
// ------------------------------------------------------------
bool schedulerCheckEvent(const SystemConfig &config, PumpEvent &evt) {
    time_t now = time(nullptr);

    for (auto &p : config.pumps) {
        if (!p.enabled) continue;

        // Find state entry
        PumpState *state = nullptr;
        for (auto &s : STATES) {
            if (s.pump_id == p.id) {
                state = &s;
                break;
            }
        }
        if (!state) continue;

        // First run of the day
        if (state->last_run_epoch == 0) {
            evt.pump_id = p.id;
            evt.run_minutes = p.run_minutes;
            state->last_run_epoch = now;
            state->minutes_today += p.run_minutes;
            return true;
        }

        // Check interval
        uint32_t elapsed = now - state->last_run_epoch;
        uint32_t required = p.interval_minutes * 60;

        if (elapsed >= required) {
            evt.pump_id = p.id;
            evt.run_minutes = p.run_minutes;
            state->last_run_epoch = now;
            state->minutes_today += p.run_minutes;
            return true;
        }
    }

    return false;
}

// ------------------------------------------------------------
// Determine next wake time
// ------------------------------------------------------------
uint64_t schedulerNextWake(const SystemConfig &config) {
    time_t now = time(nullptr);
    uint64_t nextWake = UINT64_MAX;

    for (auto &p : config.pumps) {
        if (!p.enabled) continue;

        // Find state
        PumpState *state = nullptr;
        for (auto &s : STATES) {
            if (s.pump_id == p.id) {
                state = &s;
                break;
            }
        }
        if (!state) continue;

        uint32_t last = state->last_run_epoch;
        uint32_t interval = p.interval_minutes * 60;

        uint64_t next = (last == 0)
                        ? 5 * 60   // first run soon after boot
                        : (last + interval) - now;

        if (next < nextWake) nextWake = next;
    }

    // Safety clamp
    if (nextWake < 60) nextWake = 60;        // never wake too fast
    if (nextWake > 6 * 3600) nextWake = 3600; // never sleep >1 hour

    return nextWake;
}

// ------------------------------------------------------------
// Reset daily counters (called at sunset)
// ------------------------------------------------------------
void resetDailyPumpCounters() {
    for (auto &s : STATES) {
        s.minutes_today = 0;
        s.last_run_epoch = 0;
    }
}