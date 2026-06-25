//
// Created by dunamis on 30/04/2026.
//

#ifndef SMARTDRIVE_STDMUTEX_H
#define SMARTDRIVE_STDMUTEX_H

#ifndef ARDUINO
#include <mutex>

#include "opx/shared/interfaces/IMutex.h"

class StdMutex : public IMutex {
    std::mutex mutex;
public:
    void lock() override {mutex.lock();}
    void unlock() override {mutex.unlock();}
};
#endif


#endif //SMARTDRIVE_STDMUTEX_H