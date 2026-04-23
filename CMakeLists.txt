cmake_minimum_required(VERSION 3.16)
project(SmartDrive VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ARDUINO_LIBRARY_PATH "$ENV{HOME}/Arduino/libraries/SmartDrive/src")
set(PREVIOUS_GENERATED_CONFIG "")

find_package(Boost REQUIRED COMPONENTS system)

# Build the tool
add_subdirectory(tools/CommandGenerator)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/include/smartdrive/generated)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/generated)

# Discover all device JSON files in devices/
file(GLOB DEVICE_JSON_FILES "${CMAKE_SOURCE_DIR}/devices/*.json")

# For each device JSON, register a custom command to generate its files
foreach(DEVICE_JSON ${DEVICE_JSON_FILES})
    # Extract device name from filename e.g. IndicatorBoard.json -> IndicatorBoard
    get_filename_component(DEVICE_NAME ${DEVICE_JSON} NAME_WE)

    add_custom_command(
            OUTPUT
            ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/CommandTypes.h
            ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/CommandPacker.h
            ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/${DEVICE_NAME}Controller.h
            ${CMAKE_SOURCE_DIR}/src/generated/CommandRegistry.cpp
            ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/GeneratedConfig.h
            COMMAND CommandGenerator
            ${DEVICE_JSON}
            ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/
            ${CMAKE_SOURCE_DIR}/src/generated/
            DEPENDS
            ${DEVICE_JSON}
            CommandGenerator
            ${PREVIOUS_GENERATED_CONFIG}
            COMMENT "Generating files for device: ${DEVICE_NAME}..."
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    # Each iteration depends on the previous one's GeneratedConfig output
    set(PREVIOUS_GENERATED_CONFIG
            ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/GeneratedConfig.h
    )

    list(APPEND ALL_GENERATED_HEADERS
            ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/${DEVICE_NAME}Controller.h
    )
endforeach()

add_custom_target(GenerateCommands ALL
        DEPENDS
        ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/CommandTypes.h
        ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/CommandPacker.h
        ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/GeneratedConfig.h
        ${CMAKE_SOURCE_DIR}/src/generated/CommandRegistry.cpp
        ${ALL_GENERATED_HEADERS}
)

add_library(SmartDrive
        src/generated/CommandRegistry.cpp
        include/smartdrive/core/platform.h
        include/smartdrive/interfaces/ITransport.h
        include/smartdrive/utils/CommandQueue.h
        include/smartdrive/core/CommunicationManager.h
        include/smartdrive/transport/AbstractTransport.h
        include/smartdrive/transport/serial/ArduinoSerialTransport.h
        include/smartdrive/transport/serial/PcSerialTransport.h
        include/smartdrive/generated/IndicatorBoardController.h
        include/smartdrive/core/string.hpp
        include/smartdrive/utils/CRC8.h
        include/smartdrive/interfaces/IEncoder.h
        include/smartdrive/protocol/BinaryEncoder.h
        tests/transport/serial_receive_single.cpp
        tests/transport/serial_receive_multithreaded.cpp
        include/smartdrive/utils/PendingAckQueue.h
        include/smartdrive/utils/ResponseQueue.h
        include/smartdrive/interfaces/IPlatformClock.h
        include/smartdrive/core/PlatformClock.h
        include/smartdrive/core/TriggerConfig.h
        include/smartdrive/core/TelemetryManager.h
        tests/transport/serial_telemetry_receive.cpp
)

add_custom_target(SyncArduinoLibrary ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/include/smartdrive
        ${ARDUINO_LIBRARY_PATH}/smartdrive
        COMMENT "Syncing SmartDrive headers to Arduino library..."
)

add_dependencies(SyncArduinoLibrary SmartDrive GenerateCommands)

target_include_directories(SmartDrive PUBLIC include)
target_include_directories(SmartDrive PRIVATE ${CMAKE_SOURCE_DIR}/external/httplib)
target_link_libraries(SmartDrive PUBLIC Boost::system)

# Library should wait for generator to finish first
add_dependencies(SmartDrive GenerateCommands)

#add_executable(SmartDrive_Test_BinaryEncoder
#        tests/protocol/test_binary_encoder.cpp
#)

add_executable(SmartDrive_Test_Command
        tests/test_command_registry.cpp
)

add_executable(SmartDrive_Test_SerialTransport
#        tests/transport/test_serial_transport.cpp
#        tests/transport/serial_round_trip.cpp
#        tests/transport/serial_receive_single.cpp
#        tests/transport/serial_receive_multithreaded.cpp
#        tests/transport/serial_command_response.cpp
        tests/transport/serial_telemetry_receive.cpp
)

#target_link_libraries(SmartDrive_Test_BinaryEncoder PRIVATE SmartDrive)
target_link_libraries(SmartDrive_Test_Command PRIVATE SmartDrive)
target_link_libraries(SmartDrive_Test_SerialTransport PRIVATE SmartDrive)