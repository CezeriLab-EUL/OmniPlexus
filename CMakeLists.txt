cmake_minimum_required(VERSION 3.16)
project(SmartDrive VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the tool
add_subdirectory(tools/CommandGenerator)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/include/smartdrive/generated)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/generated)

# Auto-run generator when commands.json changes
add_custom_command(
        OUTPUT
        ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/CommandTypes.h
        ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/CommandPacker.h
        ${CMAKE_SOURCE_DIR}/src/generated/CommandRegistry.cpp
        COMMAND CommandGenerator
        ${CMAKE_SOURCE_DIR}/commands.json
        ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/
        ${CMAKE_SOURCE_DIR}/src/generated/
        DEPENDS
        ${CMAKE_SOURCE_DIR}/commands.json
        CommandGenerator
        COMMENT "Generating command files from commands.json..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Create a target for the generated files
add_custom_target(GenerateCommands ALL
        DEPENDS
        ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/CommandTypes.h
        ${CMAKE_SOURCE_DIR}/include/smartdrive/generated/CommandPacker.h
        ${CMAKE_SOURCE_DIR}/src/generated/CommandRegistry.cpp
)

add_library(SmartDrive
        src/generated/CommandRegistry.cpp
        include/smartdrive/core/platform.h
)

target_include_directories(SmartDrive PUBLIC include)

# Library should wait for generator to finish first
add_dependencies(SmartDrive GenerateCommands)

add_executable(SmartDrive_Test_BinaryEncoder
        tests/protocol/test_binary_encoder.cpp
)

add_executable(SmartDrive_Test_Command
        tests/test_command_registry.cpp
)

target_link_libraries(SmartDrive_Test_BinaryEncoder PRIVATE SmartDrive)
target_link_libraries(SmartDrive_Test_Command PRIVATE SmartDrive)