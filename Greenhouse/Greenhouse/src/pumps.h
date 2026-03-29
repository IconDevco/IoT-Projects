// pumps.h
#pragma once
#include <Arduino.h>
#include "config.h"
#include "scheduler.h"

void pumpsInit(const SystemConfig &config);
void pumpOn(int pin);
void pumpOff(int pin);
void runPumpEvent(const PumpEvent &evt, const SystemConfig &config);