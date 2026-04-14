//
// Created by dunamis on 27/01/2026.
//

#ifndef SMARTDRIVE_ROBOTDATA_H
#define SMARTDRIVE_ROBOTDATA_H

#include "../core/platform.h"
#include "../core/ValueSource.h"

#pragma pack(push, 1)
struct SettingsData : public ValueSource
{
    uint16_t settingsID;
};

struct Telemetry : public ValueSource
{
    uint16_t sourceID;
};
#pragma pack(pop)

static_assert(sizeof(Telemetry) == 19, "TelemetryData must be exactly 23 bytes");
#endif // SMARTDRIVE_ROBOTDATA_H