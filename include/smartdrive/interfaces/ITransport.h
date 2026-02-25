//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_ITRANSPORT_H
#define SMARTDRIVE_ITRANSPORT_H

#include "smartdrive/types/ProtocolTypes.h"

class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool send(const SerializedData& data) = 0;
    virtual void accumulate() = 0;
    virtual bool hasCompleteFrame() const =0;
    virtual RawData getFrame() = 0;
};

#endif //SMARTDRIVE_ITRANSPORT_H