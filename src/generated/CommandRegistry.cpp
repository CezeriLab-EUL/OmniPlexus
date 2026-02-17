//
// Created by dunamis on 16/02/2026.
//

#include "../../include/smartdrive/registry/CommandRegistry.h"

#ifndef EMBEDDED_BUILD

void CommandRegistry::initialize() {
    registerCommand({
        0x0001,
        "STOP",
        "Emergency stop",
        {}
    });

    registerCommand({
        0x0002,
        "MOVE",
        "Move to coordinates",
        {
            {"distance", ValueType::FLOAT, true, "Distance", 0, ""}
        }
    });
}

#endif
