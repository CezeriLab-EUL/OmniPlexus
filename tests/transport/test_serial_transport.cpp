//
// Created by dunamis on 25/02/2026.
//

#include <iostream>

#include "smartdrive/transport/PcSerialTransport.h"
#include "smartdrive/protocol/BinaryEncoder.h"

int main() {
    PcSerialTransport sender("/dev/pts/4", 115200);
    PcSerialTransport receiver("/dev/pts/5", 115200);

    BinaryEncoder encoder;

    Command cmd;
    cmd.commandType  = CommandType::MOVE;
    cmd.params[0] = 15.5f; // distance
    cmd.params[1] = 2.0f;

    SerializedData frame = encoder.serializeCommand(cmd);
    bool success = sender.send(frame);
    std::cout << "Sent: " << success << std::endl;

    Command received;
    for (int i=0; i<1000; i++) {
        receiver.accumulate();
        bool completeFrame = receiver.hasCompleteFrame();
        std::cout << "Received frame: " << completeFrame << std::endl;
        if (completeFrame) {
            RawData raw = receiver.getFrame();
            encoder.deserializeCommand(raw, received);
            break;
        }
    }

    std::cout << "Received: " << received.commandType << std::endl;
    std::cout << "Received param 1: " << static_cast<float>(received.params[0]) << std::endl;
    std::cout << "Received param 2: " << static_cast<float>(received.params[1]) << std::endl;

    return 0;
}