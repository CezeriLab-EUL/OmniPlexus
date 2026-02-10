add_executable(SmartDrive_Test
        tests/protocol/test_binary_encoder.cpp
        include/smartdrive/protocol/BinaryEncoder.h
        include/smartdrive/utils/Logger.h
)

target_include_directories(SmartDrive_Test PRIVATE include)