//
// Created by dunamis on 25/02/2026.
//

#pragma once

#include "GeneratedConfig.h"
#include "opx/shared/core/Config.h"
#include "opx/shared/types/ProtocolTypes.h" // IWYU pragma: keep
#include "opx/shared/utils/Logger.h"

#ifdef OPX_TARGET_PC
#include <mutex>
#endif

struct PackedCommand {
  uint8_t paramBytes[MAX_PACKED_PARAM_SIZE];
  ;
  uint8_t paramSize;
  uint8_t seqNum;
  uint8_t sourceTransportID;
};

class CommandQueue {
public:
  static constexpr uint8_t CAPACITY = COMMAND_QUEUE_CAPACITY;

private:
  PackedCommand buffer[CAPACITY];
  uint8_t head = 0;
  uint8_t tail = 0;
  uint8_t count = 0;

#ifdef OPX_FRAMEWORK_ARDUINO
#if OPX_HAS_FREERTOS
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  void lock() { taskENTER_CRITICAL(&mux); }
  void unlock() { taskEXIT_CRITICAL(&mux); }
#elif OPX_TARGET_AVR
  uint8_t savedSREG = 0;
  void lock() {
    savedSREG = SREG;
    cli();
  }
  void unlock() { SREG = savedSREG; }
#else
  void lock() { noInterrupts(); }
  void unlock() { interrupts(); }
#endif
#else
  std::mutex mtx;
  void lock() { mtx.lock(); }
  void unlock() { mtx.unlock(); }
#endif

public:
  CommandQueue() = default;

  void push(const PackedCommand &cmd) {
    lock();
    if (count == CAPACITY) {
      LOG(LogLevel::OP_WARNING,
          "CommandQueue is full, dropping oldest command.");
      head = (head + 1) % CAPACITY;
      count--;
    }
    buffer[tail] = cmd;
    tail = (tail + 1) % CAPACITY;
    count++;
    unlock();
  }

  bool pop(PackedCommand &cmdOut) {
    lock();
    if (count == 0) {
      unlock();
      return false;
    }

    cmdOut = buffer[head];
    head = (head + 1) % CAPACITY;
    count--;
    unlock();
    return true;
  }

  bool isEmpty() const { return count == 0; }

  bool isFull() const { return count == CAPACITY; }

  uint8_t size() const { return count; }

  void clear() {
    lock();
    head = tail = count = 0;
    unlock();
  }
};
