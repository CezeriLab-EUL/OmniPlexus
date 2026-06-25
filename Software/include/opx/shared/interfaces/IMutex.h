//
// Created by dunamis on 30/04/2026.
//

#pragma once

class IMutex {
public:
  virtual ~IMutex() = default;
  virtual void lock() = 0;
  virtual void unlock() = 0;
};
