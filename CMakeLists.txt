add_executable(SmartDrive_Test
        tests/protocol/test_binary_encoder.cpp
        include/smartdrive/protocol/BinaryEncoder.h
        include/smartdrive/utils/Logger.h
        include/smartdrive/generated/CommandTypes.h
        include/smartdrive/generated/CommandPacker.h
        include/smartdrive/registry/CommandRegistry.h
)

target_include_directories(SmartDrive_Test PRIVATE include)