//
// Created by dunamis on 11/03/2026.
//

#ifndef SMARTDRIVE_RESPONSEQUEUE_H
#define SMARTDRIVE_RESPONSEQUEUE_H

#include "opx/shared/core/Config.h"
#include "opx/shared/types/ProtocolTypes.h"
#include "opx/shared/utils/Logger.h"

#if !defined(ARDUINO) && !defined(ESP_PLATFORM)
#include <mutex>
#endif

struct QueuedResponse {
  CommandResponse response;
  uint8_t sourceTransportID;
};

class ResponseQueue {
public:
  static constexpr uint8_t CAPACITY = PENDING_ACK_CAPACITY;

private:
  QueuedResponse buffer[CAPACITY];
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
  ResponseQueue() = default;

  bool push(const CommandResponse &response, uint8_t sourceTransportID = 0) {
    lock();
    if (count == CAPACITY) {
      LOG(LogLevel::OP_WARNING, "ResponseQueue is full, cannot track response");
      unlock();
      return false;
    }
    buffer[tail] = {response, sourceTransportID};
    tail = (tail + 1) % CAPACITY;
    count++;
    unlock();
    return true;
  }

  bool pop(QueuedResponse &responseOut) {
    lock();
    if (count == 0) {
      unlock();
      return false;
    }
    responseOut = buffer[head];
    head = (head + 1) % CAPACITY;
    count--;
    unlock();
    return true;
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

#endif // SMARTDRIVE_RESPONSEQUEUE_H