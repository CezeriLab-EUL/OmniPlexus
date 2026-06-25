//
// Created by dunamis on 30/04/2026.
//

#pragma once
#include "opx/shared/interfaces/IMutex.h"

class NullMutex : public IMutex {
public:
  void lock() override {}
  void unlock() override {}
};
