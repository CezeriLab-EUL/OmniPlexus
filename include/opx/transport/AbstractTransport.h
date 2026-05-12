//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_ABSTRACTTRANSPORT_H
#define SMARTDRIVE_ABSTRACTTRANSPORT_H

#include "opx/interfaces/ITransport.h"
#include "opx/utils/CRC8.h"
#include "opx/generated/CommandPacker.h"
#include "opx/core/ValueSource.h"

class AbstractTransport : public ITransport {
private:
    enum class AccumulatorState : uint8_t {
        WAITING_FOR_HEADER,
        READING_PREAMBLE,
        READING_STRING_TYPESIZE,
        READING_REMAINDER,
        VALIDATING_CRC,
        FRAME_READY
    };

    uint8_t accumulateBuffer[ProtocolConstants::MAX_FRAME_SIZE];
    uint8_t readyBuffer[ProtocolConstants::MAX_FRAME_SIZE];
    bool frameConsumed = true;
    uint8_t bytesCollected = 0;
    AccumulatorState state = AccumulatorState::WAITING_FOR_HEADER;

    ProtocolConstants::FrameType currentFrameType = ProtocolConstants::FrameType::COMMAND;
    uint8_t preambleTarget = 0;
    uint8_t remainderTarget = 0;
    uint8_t stringBytesUntilTypeAndSize = 0;

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
    uint8_t computeRemainingPayload() {
        if (currentFrameType == ProtocolConstants::FrameType::RESPONSE) {
            return sizeof(CommandResponse);
        }

        if (currentFrameType == ProtocolConstants::FrameType::TELEMETRY) {
            // Buffer layout: [0]=header, [1..2]=sourceID, [3]=typeAndSize
            ValueSource temp;
            temp.setTypeAndSizeRaw(accumulateBuffer[3]);
            return static_cast<uint8_t>(temp.getDataSize());
        }

        if (currentFrameType == ProtocolConstants::FrameType::COMMAND) {
            // Buffer layout: [0]=header, [1]=seqNum, [2..3]=commandType (little-endian)
            const uint16_t commandType =
                    static_cast<uint16_t>(accumulateBuffer[2]) |
                    (static_cast<uint16_t>(accumulateBuffer[3]) << 8);
            const uint8_t size = CommandPacker::packedSize(commandType);

            if (size == ProtocolConstants::STRING_SENTINEL) {
                return ProtocolConstants::STRING_SENTINEL;
            }

            // pack() output includes commandType(2) at front; preamble already read it.
            // remainder = everything pack() writes after the commandType prefix.
            return (size >= 2) ? (size - 2) : 0;
        }

        return 0;
    }

    void processByte(const uint8_t byte) {
        if (bytesCollected >= ProtocolConstants::MAX_FRAME_SIZE) {
            LOG(LogLevel::OP_ERROR, "Frame buffer overflow, resetting");
            reset();
            return;
        }

        switch (state) {
            case AccumulatorState::WAITING_FOR_HEADER: {
                if (!ProtocolConstants::isValidHeader(byte) || !ProtocolConstants::isValidFrameType(byte)) {
                    LOG(LogLevel::OP_WARNING, "Discarding invalid header byte");
                    break;
                }
                bytesCollected = 0;
                accumulateBuffer[bytesCollected++] = byte;
                currentFrameType = ProtocolConstants::decodeType(byte);

                if (currentFrameType == ProtocolConstants::FrameType::RESPONSE) {
                    remainderTarget = computeRemainingPayload();
                    state = AccumulatorState::READING_REMAINDER;
                } else {
                    preambleTarget = (currentFrameType == ProtocolConstants::FrameType::COMMAND)
                                         ? ProtocolConstants::COMMAND_PREAMBLE_SIZE
                                         : ProtocolConstants::TELEMETRY_PREAMBLE_SIZE;
                    state = AccumulatorState::READING_PREAMBLE;
                }
                break;
            }

            case AccumulatorState::READING_PREAMBLE: {
                accumulateBuffer[bytesCollected++] = byte;
                if (bytesCollected == 1 + preambleTarget) {
                    const uint8_t remaining = computeRemainingPayload();

                    if (remaining == ProtocolConstants::STRING_SENTINEL) {
                        const uint16_t commandType = static_cast<uint16_t>(accumulateBuffer[2]) |
                                                     (static_cast<uint16_t>(accumulateBuffer[3]) << 8);
                        // How many bytes from start-of-payload until typeAndSize?
                        // We've already collected preambleTarget bytes after header.
                        // stringParamOffset is from start-of-payload (index 1).
                        // Bytes already collected after header = preambleTarget (= COMMAND_PREAMBLE_SIZE = 3).
                        const uint8_t offset = CommandPacker::stringParamOffset(commandType);
                        stringBytesUntilTypeAndSize = offset - preambleTarget;
                        state = AccumulatorState::READING_STRING_TYPESIZE;
                    } else if (remaining == 0) {
                        state = AccumulatorState::VALIDATING_CRC;
                    } else {
                        remainderTarget = remaining;
                        state = AccumulatorState::READING_REMAINDER;
                    }
                }
                break;
            }

            case AccumulatorState::READING_STRING_TYPESIZE: {
                accumulateBuffer[bytesCollected++] = byte;

                if (stringBytesUntilTypeAndSize > 0) {
                    stringBytesUntilTypeAndSize--;
                    break;
                }

                ValueSource temp;
                temp.setTypeAndSizeRaw(byte);
                const uint8_t strDataSize = static_cast<uint8_t>(temp.getDataSize());
                remainderTarget = strDataSize;

                if (remainderTarget == 0) {
                    state = AccumulatorState::VALIDATING_CRC;
                } else {
                    state = AccumulatorState::READING_REMAINDER;
                }

                break;
            }

            case AccumulatorState::READING_REMAINDER: {
                accumulateBuffer[bytesCollected++] = byte;
                remainderTarget--;

                if (remainderTarget == 0) {
                    state = AccumulatorState::VALIDATING_CRC;
                }

                break;
            }

            case AccumulatorState::VALIDATING_CRC: {
                const uint8_t receivedCRC = byte;
                accumulateBuffer[bytesCollected++] = byte;
                RawData toCheck{accumulateBuffer, static_cast<size_t>(bytesCollected - 1)};
                const uint8_t calculatedCRC = CRC8::compute(toCheck);
                if (receivedCRC != calculatedCRC) {
                    LOG(LogLevel::OP_ERROR, "CRC mismatch, resetting");
                    reset();
                } else {
                    if (!frameConsumed) {
                        LOG(LogLevel::OP_WARNING, "Previous frame not released, dropping new frame");
                        reset();
                    } else {
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
        preambleTarget = 0;
        remainderTarget = 0;
        stringBytesUntilTypeAndSize = 0;
        currentFrameType = ProtocolConstants::FrameType::COMMAND;
        state = AccumulatorState::WAITING_FOR_HEADER;
    }
};

#endif //SMARTDRIVE_ABSTRACTTRANSPORT_H
