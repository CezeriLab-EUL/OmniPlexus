//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_ABSTRACTTRANSPORT_H
#define SMARTDRIVE_ABSTRACTTRANSPORT_H

#include "smartdrive/interfaces/ITransport.h"

class AbstractTransport : public ITransport {
private:
    enum class AccumulatorState : uint8_t {
        WAITING_FOR_HEADER,
        WAITING_FOR_LENGTH,
        READING_PAYLOAD,
        VALIDATING_CRC,
        FRAME_READY
    };

    uint8_t frameBuffer[ProtocolConstants::MAX_FRAME_SIZE];
    uint8_t bytesCollected = 0;
    uint8_t payloadLength = 0;
    AccumulatorState state = AccumulatorState::WAITING_FOR_HEADER;

protected:
    virtual uint16_t bytesAvailable() = 0;
    virtual uint8_t readByte() = 0;

public:
    bool send(const SerializedData &data) override = 0;

    void accumulate() override {
        while (bytesAvailable() > 0) {
            const uint8_t byte = readByte();
            processByte(byte);

            if (state == AccumulatorState::FRAME_READY) {
                break;
            }
        }
    }

    bool hasCompleteFrame() const override {
        return state == AccumulatorState::FRAME_READY;
    }

    RawData getFrame() override {
        RawData result;
        result.data = frameBuffer;
        result.size = bytesCollected;

        reset();
        // The caller must consume  result.data before the next call to accumulate(),
        // as accumulate() will overwrite the buffer.
        return result;
    }

private:
    void processByte(const uint8_t byte) {
        switch (state) {
            case AccumulatorState::WAITING_FOR_HEADER: {
                if (ProtocolConstants::isValidHeader(byte)) {
                    bytesCollected = 0;
                    frameBuffer[bytesCollected++] = byte;
                    state = AccumulatorState::WAITING_FOR_LENGTH;
                }else {
                    LOG(LogLevel::WARNING, "Discarding invalid header byte");
                }
                break;
            }

            case AccumulatorState::WAITING_FOR_LENGTH: {
                if (byte == 0 || byte > ProtocolConstants::MAX_PAYLOAD_SIZE) {
                    LOG(LogLevel::ERROR, "Invalid payload length, resetting");
                    reset();
                    break;
                }

                payloadLength = byte;
                frameBuffer[bytesCollected++] = byte;
                state = AccumulatorState::READING_PAYLOAD;
                break;
            }

            case AccumulatorState::READING_PAYLOAD: {
                frameBuffer[bytesCollected++] = byte;

                if (bytesCollected == 2 + payloadLength) {
                    state = AccumulatorState::VALIDATING_CRC;
                }
                break;
            }

            case AccumulatorState::VALIDATING_CRC: {
                const uint8_t receivedCRC = byte;
                frameBuffer[bytesCollected++] = byte;

                RawData toCheck {frameBuffer, static_cast<size_t>(bytesCollected - 1)};
                const uint8_t calculatedCRC = computeCRC8(toCheck);

                if (receivedCRC != calculatedCRC) {
                    LOG(LogLevel::ERROR, "CRC mismatch, resetting");
                    reset();
                }else {
                    state = AccumulatorState::FRAME_READY;
                }
                break;
            }

            case AccumulatorState::FRAME_READY: {
                break;
            }
        }
    }

    void reset() {
        bytesCollected = 0;
        payloadLength = 0;
        state = AccumulatorState::WAITING_FOR_HEADER;
    }

    static uint8_t computeCRC8(const RawData& rawData) {
        uint8_t crc = 0x00;
        for (size_t i = 0; i < rawData.size; ++i) {
            crc ^= rawData.data[i];
            for (uint8_t bit = 0; bit < 8; ++bit) {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ 0x07;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }
};

#endif //SMARTDRIVE_ABSTRACTTRANSPORT_H