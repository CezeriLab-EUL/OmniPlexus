//
// Created by dunamis on 09/03/2026.
//

#ifndef SMARTDRIVE_PENDINGACKQUEUE_H
#define SMARTDRIVE_PENDINGACKQUEUE_H

#include "opx/shared/core/Config.h"
#include "opx/shared/utils/Logger.h"

#if !defined(ARDUINO) && !defined(ESP_PLATFORM)
#include <mutex>
#endif

struct PendingAckEntry {
  uint8_t seqNum;
  uint16_t commandType;
};

class PendingAckQueue {
public:
  static constexpr uint8_t CAPACITY = PENDING_ACK_CAPACITY;

private:
  PendingAckEntry buffer[CAPACITY];
  uint8_t head = 0;
  uint8_t tail = 0;
  uint8_t count = 0;

#if defined(ARDUINO)
#if defined(ESP32) || defined(ESP8266)
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  void lock() { taskENTER_CRITICAL(&mux); }
  void unlock() { taskEXIT_CRITICAL(&mux); }
#elif defined(__AVR__)
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
  PendingAckQueue() = default;

  bool push(const PendingAckEntry &entry) {
    lock();
    if (count == CAPACITY) {
      LOG(LogLevel::OP_WARNING,
          "PendingAckQueue is full, cannot track command");
      unlock();
      return false;
    }
    buffer[tail] = entry;
    tail = (tail + 1) % CAPACITY;
    count++;
    unlock();
    return true;
  }

  bool resolve(const uint8_t seqNum, const uint16_t commandType) {
    lock();
    for (uint8_t i = 0; i < count; i++) {
      uint8_t idx = (head + i) % CAPACITY;
      if (buffer[idx].seqNum == seqNum &&
          buffer[idx].commandType == commandType) {
        for (uint8_t j = i; j < count - 1; j++) {
          uint8_t current = (head + j) % CAPACITY;
          uint8_t next = (head + j + 1) % CAPACITY;
          buffer[current] = buffer[next];
        }
        tail = (tail == 0) ? CAPACITY - 1 : tail - 1;
        count--;
        unlock();
        return true;
      }
    }
    unlock();
    return false;
  }

  bool isEmpty() const { return count == 0; }
  uint8_t size() const { return count; }
  bool isFull() const { return count == CAPACITY; }

  void clear() {
    lock();
    head = tail = count = 0;
    unlock();
  }
};

#endif // SMARTDRIVE_PENDINGACKQUEUE_H