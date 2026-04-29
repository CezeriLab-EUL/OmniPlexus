//
// Created by dunamis on 30/04/2026.
//

#ifndef SMARTDRIVE_IMUTEX_H
#define SMARTDRIVE_IMUTEX_H

class IMutex {
public:
    virtual ~IMutex() = default;
    virtual void lock() = 0;
    virtual void unlock() = 0;
};

#endif //SMARTDRIVE_IMUTEX_H