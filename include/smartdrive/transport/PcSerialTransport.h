//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_PCSERIALTRANSPORT_H
#define SMARTDRIVE_PCSERIALTRANSPORT_H

#ifndef ARDUINO

#include "smartdrive/transport/AbstractTransport.h"
#include <boost/asio.hpp>

class PcSerialTransport : public AbstractTransport {
private:
    boost::asio::io_context ioContext;
    boost::asio::serial_port serialPort;

    uint8_t incomingByte = 0;
    bool byteReady = false;
    bool readInProgress = false;
public:
    PcSerialTransport(const std::string& portName, uint32_t baudRate) : serialPort(ioContext) {
        serialPort.open(portName);
        serialPort.set_option(boost::asio::serial_port_base::baud_rate(baudRate));
        serialPort.set_option(boost::asio::serial_port_base::character_size(8));
        serialPort.set_option(boost::asio::serial_port_base::stop_bits(
            boost::asio::serial_port_base::stop_bits::one));
        serialPort.set_option(boost::asio::serial_port_base::parity(
            boost::asio::serial_port_base::parity::none));
        serialPort.set_option(boost::asio::serial_port_base::flow_control(
            boost::asio::serial_port_base::flow_control::none));
    }

    ~PcSerialTransport() {
        if (serialPort.is_open()) {
            serialPort.close();
        }
    }

    bool send(const SerializedData &data) override {
        boost::system::error_code ec;
        const size_t bytesWritten = boost::asio::write(
            serialPort,
            boost::asio::buffer(data.data, data.size),
            ec);

        if (ec) {
            return false;
        }

        return bytesWritten == data.size;
    }

protected:
    uint16_t bytesAvailable() override {
        if (byteReady) {
            return 1;
        }

        if (!readInProgress) {
            readInProgress = true;
            serialPort.async_read_some(
                boost::asio::buffer(&incomingByte, 1),
                [this](const boost::system::error_code& asyncEc, size_t bytesRead) {
                    readInProgress = false;
                    if (!asyncEc && bytesRead > 0) {
                        byteReady = true;
                    }
                });
        }

        ioContext.poll();
        ioContext.restart();
        return byteReady ? 1 : 0;
    }

    uint8_t readByte() override {
        byteReady = false;
        return incomingByte;
    }
};
#endif

#endif //SMARTDRIVE_PCSERIALTRANSPORT_H