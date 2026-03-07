//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_ABSTRACTTRANSPORT_H
#define SMARTDRIVE_ABSTRACTTRANSPORT_H

#include "smartdrive/interfaces/ITransport.h"
#include "smartdrive/utils/CRC8.h"

class AbstractTransport : public ITransport {
private:
    enum class AccumulatorState : uint8_t {
        WAITING_FOR_HEADER,
        WAITING_FOR_LENGTH,
        READING_PAYLOAD,
        VALIDATING_CRC,
        FRAME_READY
    };

    uint8_t accumulateBuffer[ProtocolConstants::MAX_FRAME_SIZE];
    uint8_t readyBuffer[ProtocolConstants::MAX_FRAME_SIZE];
    bool frameConsumed = true;
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
        result.data = readyBuffer;
        result.size = bytesCollected;
        return result;
    }

    void releaseFrame() override {
        frameConsumed = true;
        reset();
    }

private:
    void processByte(const uint8_t byte) {
        switch (state) {
            case AccumulatorState::WAITING_FOR_HEADER: {
                if (ProtocolConstants::isValidHeader(byte)) {
                    bytesCollected = 0;
                    accumulateBuffer[bytesCollected++] = byte;
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
                accumulateBuffer[bytesCollected++] = byte;
                state = AccumulatorState::READING_PAYLOAD;
                break;
            }

            case AccumulatorState::READING_PAYLOAD: {
                accumulateBuffer[bytesCollected++] = byte;

                if (bytesCollected == 2 + payloadLength) {
                    state = AccumulatorState::VALIDATING_CRC;
                }
                break;
            }

            case AccumulatorState::VALIDATING_CRC: {
                const uint8_t receivedCRC = byte;
                accumulateBuffer[bytesCollected++] = byte;

                RawData toCheck {accumulateBuffer, static_cast<size_t>(bytesCollected - 1)};
                const uint8_t calculatedCRC = CRC8::compute(toCheck);

                if (receivedCRC != calculatedCRC) {
                    LOG(LogLevel::ERROR, "CRC mismatch, resetting");
                    reset();
                }else {
                    if (!frameConsumed) {
                        LOG(LogLevel::WARNING, "Previous frame not released, dropping new frame");
                        reset();
                    }else {
                        memcpy(readyBuffer, accumulateBuffer, bytesCollected);
                        frameConsumed = false;
                        state = AccumulatorState::FRAME_READY;
                    }
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
};

#endif //SMARTDRIVE_ABSTRACTTRANSPORT_H