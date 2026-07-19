#pragma once

#include <Arduino.h>
#include "Types.h"

float clampFloat(float value, float minValue, float maxValue);
bool isValidPin(uint8_t pin);

const char *powerStateToText(PowerState state);
const char *learnCycleToText(LearnCycle cycle);
const char *learnStatusToText(LearnStatus status);

String formatDurationCompact(float hours);
String formatDurationForPage(float hours);
String formatSmallPowerW(float powerW);
String formatSignedFloat(float value, uint8_t digits, const char *unit);
