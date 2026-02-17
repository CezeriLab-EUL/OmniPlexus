add_library(SmartDrive
        src/generated/CommandRegistry.cpp
)

target_include_directories(SmartDrive PUBLIC include)

add_executable(SmartDrive_Test_BinaryEncoder
        tests/protocol/test_binary_encoder.cpp
)

add_executable(SmartDrive_Test_Command
        tests/test_command_registry.cpp
)

target_link_libraries(SmartDrive_Test_BinaryEncoder PRIVATE SmartDrive)
target_link_libraries(SmartDrive_Test_Command PRIVATE SmartDrive)
