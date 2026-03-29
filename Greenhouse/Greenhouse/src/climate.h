// climate.h
#pragma once
#include <Arduino.h>
#include "config.h"

// Parsed climate data
struct ClimateData {
    int temp_f = 75;
    int humidity = 40;
    int uv_index = 5;
    int cloud_cover = 20;
};

bool fetchClimateRSS(const SystemConfig &config, ClimateData &out);
void applyClimateAdjustments(SystemConfig &config, const ClimateData &climate);