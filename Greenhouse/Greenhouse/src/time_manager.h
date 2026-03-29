// time_manager.h
#pragma once
#include <Arduino.h>
#include "config.h"

// Sunrise/sunset storage
struct SunTimes {
    String sunrise;
    String sunset;
};

enum BootReason {
    BOOT_UNKNOWN,
    BOOT_POWER_ON,
    BOOT_DEEP_SLEEP,
    BOOT_BROWNOUT,
    BOOT_WATCHDOG
};

bool syncTime();
bool fetchSunTimes(const SystemConfig &config);
bool isMorning();
bool isSunset();
BootReason getBootReason();
const char* bootReasonToString(BootReason r);

// Global sunrise/sunset storage
extern SunTimes SUN;