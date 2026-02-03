add_executable(SmartDrive_Test
        tests/protocol/test_binary_protocol.cpp
        include/smartdrive/protocol/BinaryProtocol.h
        include/smartdrive/utils/Logger.h
)

target_include_directories(SmartDrive_Test PRIVATE include)