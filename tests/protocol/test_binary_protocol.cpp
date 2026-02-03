#include <iostream>
#include <iomanip>
#include "smartdrive/protocol/BinaryProtocol.h"
#include "smartdrive/utils/Logger.h"

// Simple logger callback for console output
void consoleLogger(LogLevel level, const char* message) {
    const char* levelStr;
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO: levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARN"; break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
        default: levelStr = "UNKNOWN";
    }
    std::cout << "[" << levelStr << "] " << message << std::endl;
}

// Helper to print bytes in hex
void printHex(const uint8_t* data, size_t size, const char* label) {
    std::cout << label << " (" << size << " bytes): ";
    for (size_t i = 0; i < size; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    // Setup logger
    Logger::setCallback(consoleLogger);

    std::cout << "=== SmartDrive BinaryProtocol Test Suite ===" << std::endl;

    BinaryProtocol protocol;
    int testsPassed = 0;
    int testsFailed = 0;

    // Test 1: Command Serialization/Deserialization
    {
        std::cout << "\n--- Test 1: Command Round-Trip ---" << std::endl;

        Command originalCmd;
        originalCmd.commandType = 0x1234;
        originalCmd.w = 1.5f;
        originalCmd.x = 2.5f;
        originalCmd.y = 3.5f;
        originalCmd.z = 4.5f;
        originalCmd.s = 100;
        originalCmd.t = 200;
        originalCmd.u = 300;
        originalCmd.v = 400;

        // Serialize
        SerializedData serialized = protocol.serializeCommand(originalCmd);
        printHex(serialized.data, serialized.size, "Serialized Command");

        // Deserialize
        Command receivedCmd;
        bool success = protocol.deserializeCommand(serialized.data, serialized.size, receivedCmd);

        // Verify
        bool passed = success &&
                      receivedCmd.commandType == originalCmd.commandType &&
                      receivedCmd.w == originalCmd.w &&
                      receivedCmd.x == originalCmd.x &&
                      receivedCmd.y == originalCmd.y &&
                      receivedCmd.z == originalCmd.z &&
                      receivedCmd.s == originalCmd.s &&
                      receivedCmd.t == originalCmd.t &&
                      receivedCmd.u == originalCmd.u &&
                      receivedCmd.v == originalCmd.v;

        if (passed) {
            std::cout << "✓ PASSED: Command data matches" << std::endl;
            testsPassed++;
        } else {
            std::cout << "✗ FAILED: Command data mismatch" << std::endl;
            testsFailed++;
        }
    }

    // Test 2: Discovery Response Serialization/Deserialization
    {
        std::cout << "\n--- Test 2: Discovery Response Round-Trip ---" << std::endl;

        DiscoveryResponse originalResp;
        originalResp.moduleCount = 3;
        originalResp.modules[0] = {0x0001, 0x01, 0x00FF};
        originalResp.modules[1] = {0x0002, 0x02, 0x00AA};
        originalResp.modules[2] = {0x0003, 0x03, 0x0055};

        // Serialize
        SerializedData serialized = protocol.serializeDiscovery(originalResp);
        printHex(serialized.data, serialized.size, "Serialized Discovery");

        // Deserialize
        DiscoveryResponse receivedResp;
        bool success = protocol.deserializeDiscovery(serialized.data, serialized.size, receivedResp);

        // Verify
        bool passed = success &&
                      receivedResp.moduleCount == originalResp.moduleCount &&
                      receivedResp.modules[0].typeID == 0x0001 &&
                      receivedResp.modules[1].instanceID == 0x02 &&
                      receivedResp.modules[2].capabilitiesBitmask == 0x0055;

        if (passed) {
            std::cout << "✓ PASSED: Discovery data matches" << std::endl;
            testsPassed++;
        } else {
            std::cout << "✗ FAILED: Discovery data mismatch" << std::endl;
            testsFailed++;
        }
    }

    // Test 3: Telemetry Data Serialization/Deserialization
    {
        std::cout << "\n--- Test 3: Telemetry Data Round-Trip ---" << std::endl;

        TelemetryData originalTelem;
        originalTelem.sourceID = 0x1234;
        originalTelem.timestamp = 987654321;
        originalTelem.pack<float>(42.5f);

        // Serialize
        SerializedData serialized = protocol.serializeTelemetry(originalTelem);
        printHex(serialized.data, serialized.size, "Serialized Telemetry");

        // Deserialize
        TelemetryData receivedTelem;
        bool success = protocol.deserializeTelemetry(serialized.data, serialized.size, receivedTelem);

        // Verify
        bool passed = success &&
                      receivedTelem.sourceID == originalTelem.sourceID &&
                      receivedTelem.timestamp == originalTelem.timestamp &&
                      receivedTelem.getType() == ValueType::FLOAT &&
                      receivedTelem.unpack<float>() == 42.5f;

        if (passed) {
            std::cout << "✓ PASSED: Telemetry data matches" << std::endl;
            testsPassed++;
        } else {
            std::cout << "✗ FAILED: Telemetry data mismatch" << std::endl;
            testsFailed++;
        }
    }

    // Test 4: ValueSource with String
    {
        std::cout << "\n--- Test 4: ValueSource String Round-Trip ---" << std::endl;

        ValueSource originalValue;
        originalValue.packString("Hello Robot");

        // Serialize
        SerializedData serialized = protocol.serializeValue(originalValue);
        printHex(serialized.data, serialized.size, "Serialized ValueSource");

        // Deserialize
        ValueSource receivedValue;
        bool success = protocol.deserializeValue(serialized.data, serialized.size, receivedValue);

        // Verify
        bool passed = success &&
                      receivedValue.getType() == ValueType::STRING &&
                      std::strcmp(receivedValue.unpackString(), "Hello Robot") == 0;

        if (passed) {
            std::cout << "✓ PASSED: String value matches" << std::endl;
            testsPassed++;
        } else {
            std::cout << "✗ FAILED: String value mismatch" << std::endl;
            testsFailed++;
        }
    }

    // Test 5: CRC Corruption Detection
    {
        std::cout << "\n--- Test 5: CRC Corruption Detection ---" << std::endl;

        Command originalCmd;
        originalCmd.commandType = 0x5678;
        originalCmd.w = 10.0f;

        // Serialize
        SerializedData serialized = protocol.serializeCommand(originalCmd);

        // Corrupt one byte in the payload
        serialized.data[5] ^= 0xFF;

        // Try to deserialize
        Command receivedCmd;
        bool success = protocol.deserializeCommand(serialized.data, serialized.size, receivedCmd);

        // Should fail due to CRC mismatch
        if (!success) {
            std::cout << "✓ PASSED: Corrupted data detected" << std::endl;
            testsPassed++;
        } else {
            std::cout << "✗ FAILED: Corrupted data NOT detected" << std::endl;
            testsFailed++;
        }
    }

    // Test 6: Frame Size Validation
    {
        std::cout << "\n--- Test 6: Frame Size Validation ---" << std::endl;

        Command cmd;
        cmd.commandType = 0xABCD;

        SerializedData serialized = protocol.serializeCommand(cmd);

        // Expected size: Header(1) + Length(1) + Command(26) + CRC(2) = 30 bytes
        size_t expectedSize = 30;

        if (serialized.size == expectedSize) {
            std::cout << "✓ PASSED: Frame size is " << expectedSize << " bytes" << std::endl;
            testsPassed++;
        } else {
            std::cout << "✗ FAILED: Expected " << expectedSize
                      << " bytes, got " << serialized.size << std::endl;
            testsFailed++;
        }
    }

    // Test 7: Header Encoding
    {
        std::cout << "\n--- Test 7: Header Byte Encoding ---" << std::endl;

        Command cmd;
        SerializedData serialized = protocol.serializeCommand(cmd);

        // First byte should be 0x02 for COMMAND (type=0, STX=0x02)
        uint8_t expectedHeader = 0x02;

        if (serialized.data[0] == expectedHeader) {
            std::cout << "✓ PASSED: Header byte is 0x" << std::hex
                      << static_cast<int>(expectedHeader) << std::dec << std::endl;
            testsPassed++;
        } else {
            std::cout << "✗ FAILED: Expected header 0x" << std::hex
                      << static_cast<int>(expectedHeader) << ", got 0x"
                      << static_cast<int>(serialized.data[0]) << std::dec << std::endl;
            testsFailed++;
        }
    }

    // Summary
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << testsPassed << std::endl;
    std::cout << "Failed: " << testsFailed << std::endl;
    std::cout << "Total:  " << (testsPassed + testsFailed) << std::endl;

    return (testsFailed == 0) ? 0 : 1;
}