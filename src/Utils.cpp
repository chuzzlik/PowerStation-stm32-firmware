#include "Utils.h"
#include <math.h>

float clampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }

    if (value > maxValue) {
        return maxValue;
    }

    return value;
}

bool isValidPin(uint8_t pin) {
    return pin != 255;
}

const char *powerStateToText(PowerState state) {
    switch (state) {
        case PowerState::Charge:
            return "CHARGE";
        case PowerState::Discharge:
            return "DISCHARGE";
        case PowerState::Idle:
            return "IDLE";
    }

    return "-";
}

const char *learnCycleToText(LearnCycle cycle) {
    switch (cycle) {
        case LearnCycle::Charge:
            return "charge";
        case LearnCycle::Discharge:
            return "discharge";
        case LearnCycle::Idle:
            return "idle";
    }

    return "-";
}

const char *learnStatusToText(LearnStatus status) {
    switch (status) {
        case LearnStatus::Inactive:
            return "inactive";
        case LearnStatus::Active:
            return "active";
        case LearnStatus::Complete:
            return "complete";
        case LearnStatus::Paused:
            return "paused";
    }

    return "-";
}

String formatDurationCompact(float hours) {
    if (hours < 0.0f || isnan(hours) || isinf(hours)) {
        return "IDLE";
    }

    uint32_t totalMinutes = static_cast<uint32_t>(roundf(hours * 60.0f));
    uint32_t h = totalMinutes / 60;
    uint32_t m = totalMinutes % 60;

    String result;
    result += String(h);
    result += "h ";

    if (m < 10) {
        result += "0";
    }

    result += String(m);
    result += "m";
    return result;
}

String formatDurationForPage(float hours) {
    if (hours < 0.0f || isnan(hours) || isinf(hours)) {
        return "IDLE";
    }

    return formatDurationCompact(hours);
}

String formatSmallPowerW(float powerW) {
    if (fabsf(powerW) < 1.0f) {
        return "0W";
    }

    String result;
    result += String(fabsf(powerW), 0);
    result += "W";
    return result;
}

String formatSignedFloat(float value, uint8_t digits, const char *unit) {
    String result;

    if (value > 0.0f) {
        result += "+";
    }

    result += String(value, static_cast<unsigned int>(digits));
    result += unit;
    return result;
}
