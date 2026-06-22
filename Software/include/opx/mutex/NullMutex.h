//
// Created by dunamis on 30/04/2026.
//

#ifndef SMARTDRIVE_NULLMUTEX_H
#define SMARTDRIVE_NULLMUTEX_H

#include "opx/interfaces/IMutex.h"

class NullMutex : public IMutex {
public:
    void lock() override {}
    void unlock() override {}
};

#endif //SMARTDRIVE_NULLMUTEX_H