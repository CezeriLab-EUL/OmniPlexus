//
// Created by dunamis on 29/04/2026.
//

#ifndef SMARTDRIVE_ICONNECTABLE_H
#define SMARTDRIVE_ICONNECTABLE_H

class IConnectable {
public:
    virtual ~IConnectable() = default;
    virtual bool isConnected() const = 0;
};

#endif //SMARTDRIVE_ICONNECTABLE_H