//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_ARDUINOSERIALTRANSPORT_H
#define SMARTDRIVE_ARDUINOSERIALTRANSPORT_H

#ifdef ARDUINO
#include "smartdrive/types/ProtocolTypes.h"
#include <Arduino.h>
#include "smartdrive/transport/AbstractTransport.h"

class ArduinoSerialTransport : public AbstractTransport {
private:
    HardwareSerial& serial;
    uint32_t baudRate;
public:
    ArduinoSerialTransport(HardwareSerial& serial, uint32_t baudRate) : serial(serial), baudRate(baudRate){}

    void begin() {
        serial.begin(baudRate);
    }

    bool send(const SerializedData& data) override {
        const size_t bytesWritten = serial.write(data.data, data.size);
        return bytesWritten == data.size;
    }
protected:
    uint16_t bytesAvailable() override {
        return static_cast<uint16_t>(serial.available());
    }

    uint8_t readByte() override {
        return static_cast<uint8_t>(serial.read());
    }
};
#endif
#endif //SMARTDRIVE_ARDUINOSERIALTRANSPORT_H