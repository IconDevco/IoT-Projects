// mqtt_client.h
#pragma once
#include <Arduino.h>
#include "config.h"
#include "climate.h"
#include "time_manager.h"

bool mqttInit(const SystemConfig &config);
void mqttPublishHeartbeat();
void mqttPublishSunrise(const SystemConfig &config, const ClimateData &climate);
void mqttPublishSunset(const SystemConfig &config);
void mqttProcessUpdates(SystemConfig &config);