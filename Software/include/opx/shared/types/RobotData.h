//
// Created by dunamis on 27/01/2026.
//

#pragma once

#include "opx/shared/core/ValueSource.h"

#pragma pack(push, 1)
struct SettingsData : public ValueSource {
  uint16_t settingsID;
};

struct Telemetry : public ValueSource {
  uint16_t sourceID;
};

#pragma pack(pop)

static_assert(sizeof(Telemetry) == 19,
              "TelemetryData must be exactly 19 bytes");
static_assert(sizeof(SettingsData) == 19,
              "SettingsData must be exactly 19 bytes");
