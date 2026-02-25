//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_COMMANDQUEUE_H
#define SMARTDRIVE_COMMANDQUEUE_H

#include "../core/platform.h"
#include "../types/ProtocolTypes.h"
#include "Logger.h"

class CommandQueue {
public:
    static constexpr uint8_t CAPACITY = 8;

private:
    Command buffer[CAPACITY];
    uint8_t head = 0;
    uint8_t tail = 0;
    uint8_t count = 0;

public:
    CommandQueue() = default;

    void push(const Command& cmd) {
        if (count == CAPACITY) {
            LOG(LogLevel::WARNING, "CommandQueue is full, dropping oldest command.");
            head = (head + 1) % CAPACITY;
            count--;
        }
        buffer[tail] = cmd;
        tail = (tail + 1) % CAPACITY;
        count++;
    }

    bool pop(Command& cmdOut) {
        if (count == 0) return false;

        cmdOut = buffer[head];
        head = (head + 1) % CAPACITY;
        count--;
        return true;
    }

    bool isEmpty() const { return count == 0; }

    bool isFull() const { return count == CAPACITY; }

    uint8_t size() const { return count; }

    void clear() { head = tail = count = 0; }
};

#endif //SMARTDRIVE_COMMANDQUEUE_H