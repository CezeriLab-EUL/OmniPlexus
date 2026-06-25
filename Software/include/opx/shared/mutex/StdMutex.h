//
// Created by dunamis on 30/04/2026.
//

#pragma once

#ifndef ARDUINO
#include <mutex>

#include "opx/shared/interfaces/IMutex.h"

class StdMutex : public IMutex {
  std::mutex mutex;

public:
  void lock() override { mutex.lock(); }
  void unlock() override { mutex.unlock(); }
};
#endif
