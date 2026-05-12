cmake_minimum_required(VERSION 3.16)
project(OmniPlexus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ARDUINO_LIBRARY_PATH "$ENV{HOME}/Arduino/libraries/Opx/src")
set(PREVIOUS_GENERATED_CONFIG "")

find_package(Boost REQUIRED COMPONENTS system)

# Build the tool
add_subdirectory(tools/CommandGenerator)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/include/opx/generated)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/generated)

# Discover all device JSON files in devices/
file(GLOB DEVICE_JSON_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/devices/*.json")

# Derive per-device controller header paths from JSON filenames
set(ALL_CONTROLLER_HEADERS "")
foreach(DEVICE_JSON ${DEVICE_JSON_FILES})
    get_filename_component(DEVICE_NAME ${DEVICE_JSON} NAME_WE)
    list(APPEND ALL_CONTROLLER_HEADERS
            "${CMAKE_SOURCE_DIR}/include/opx/generated/${DEVICE_NAME}Controller.h")
endforeach()

# Single custom command — passes ALL json files at once
add_custom_command(
        OUTPUT
        ${CMAKE_SOURCE_DIR}/include/opx/generated/CommandTypes.h
        ${CMAKE_SOURCE_DIR}/include/opx/generated/CommandPacker.h
        ${CMAKE_SOURCE_DIR}/include/opx/generated/GeneratedConfig.h
        ${CMAKE_SOURCE_DIR}/include/opx/generated/TelemetrySourceIDs.h
        ${CMAKE_SOURCE_DIR}/src/generated/CommandRegistry.cpp
        ${ALL_CONTROLLER_HEADERS}
        COMMAND CommandGenerator
        ${DEVICE_JSON_FILES}
        ${CMAKE_SOURCE_DIR}/include/opx/generated/
        ${CMAKE_SOURCE_DIR}/src/generated/
        DEPENDS
        ${DEVICE_JSON_FILES}
        CommandGenerator
        COMMENT "Generating files for all devices..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(GenerateCommands ALL
        DEPENDS
        ${CMAKE_SOURCE_DIR}/include/opx/generated/CommandTypes.h
        ${CMAKE_SOURCE_DIR}/include/opx/generated/CommandPacker.h
        ${CMAKE_SOURCE_DIR}/include/opx/generated/GeneratedConfig.h
        ${CMAKE_SOURCE_DIR}/include/opx/generated/TelemetrySourceIDs.h
        ${CMAKE_SOURCE_DIR}/src/generated/CommandRegistry.cpp
        ${ALL_CONTROLLER_HEADERS}
)

add_library(OmniPlexus
        src/generated/CommandRegistry.cpp
        src/OpxSession.cpp
        src/OpxDevice.cpp
)

add_custom_target(SyncArduinoLibrary
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/include/opx
        ${ARDUINO_LIBRARY_PATH}/opx

        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/src/OpxDevice.cpp
        ${ARDUINO_LIBRARY_PATH}/OpxDevice.cpp
        COMMENT "Syncing OmniPlexus headers to Arduino library..."
)

add_dependencies(SyncArduinoLibrary OmniPlexus GenerateCommands)

target_include_directories(OmniPlexus PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(OmniPlexus PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/httplib>
)
target_link_libraries(OmniPlexus PUBLIC Boost::system)

# Library should wait for generator to finish first
add_dependencies(OmniPlexus GenerateCommands)

#add_executable(SmartDrive_Test_BinaryEncoder
##        tests/protocol/test_binary_encoder.cpp
#)
#
#add_executable(SmartDrive_Test_Command
##        tests/test_command_registry.cpp
#)

add_executable(OmniPlexus_Test_SerialTransport
#        tests/transport/test_serial_transport.cpp
#        tests/transport/serial_round_trip.cpp
#        tests/transport/serial_receive_single.cpp
#        tests/transport/serial_receive_multithreaded.cpp
#        tests/transport/serial_command_response.cpp
#        tests/transport/serial_telemetry_receive.cpp
#        tests/transport/http/send_command.cpp
#        tests/transport/http/receive_telemetry.cpp
        tests/transport/wifi/send_command.cpp
)

#target_link_libraries(SmartDrive_Test_BinaryEncoder PRIVATE SmartDrive)
#target_link_libraries(SmartDrive_Test_Command PRIVATE SmartDrive)
target_link_libraries(OmniPlexus_Test_SerialTransport PRIVATE OmniPlexus)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS OmniPlexus
        EXPORT OmniPlexusTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/opx
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT OmniPlexusTargets
        FILE OmniPlexusConfig.cmake
        NAMESPACE OmniPlexus::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OmniPlexus
)