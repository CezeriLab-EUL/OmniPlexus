//
// Created by dunamis on 26/01/2026.
//

#ifndef SMARTDRIVE_LOGGER_H
#define SMARTDRIVE_LOGGER_H

#include "../core/Config.h"

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    using LogCallback = void (*)(LogLevel level, const char *message);

#if LOGGING_ENABLED
    static void setCallback(LogCallback cb) {
        getCallback() = cb;
    }

    static void log(LogLevel level, const char *message) {
        if (getCallback()) getCallback()(level, message);
    }

private:
    static LogCallback &getCallback() {
        static LogCallback instance = nullptr;
        return instance;
    }
#else
    static void setCallback(LogCallback) {
    }
    static void log(LogLevel, const char *) {
    }
#endif
};

#if LOGGING_ENABLED
    #define LOG(level, msg) Logger::log(level, msg)
#else
    #define LOG(level, msg) ((void)0)
#endif

#endif //SMARTDRIVE_LOGGER_H
