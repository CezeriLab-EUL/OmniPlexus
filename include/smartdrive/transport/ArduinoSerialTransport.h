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
        const int available = serial.available();
        return (available > 0) ? static_cast<uint16_t>(available) : 0;
    }

    uint8_t readByte() override {
        const int byte = serial.read();
        return (byte >= 0) ? static_cast<uint8_t>(byte) : 0;
    }
};
#endif
#endif //SMARTDRIVE_ARDUINOSERIALTRANSPORT_H