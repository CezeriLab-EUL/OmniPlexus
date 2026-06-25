//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_PCSERIALTRANSPORT_H
#define SMARTDRIVE_PCSERIALTRANSPORT_H

#include "opx/shared/core/Config.h" // IWYU pragma: keep

#ifndef OPX_TARGET_EMBEDDED

#ifndef _WIN32
#include <sys/ioctl.h>
#else
#include <windows.h>
#endif

#include "opx/shared/transport/AbstractTransport.h"
#include <boost/asio.hpp>

class PcSerialTransport : public AbstractTransport {
private:
  boost::asio::io_context ioContext;
  boost::asio::serial_port serialPort;

  uint8_t stagingBuffer[ProtocolConstants::MAX_FRAME_SIZE];
  uint16_t stagingHead = 0;
  uint16_t stagingTail = 0;

public:
  PcSerialTransport(const std::string &portName, uint32_t baudRate)
      : serialPort(ioContext) {
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
        serialPort, boost::asio::buffer(data.data, data.size), ec);

    if (ec) {
      return false;
    }

    return bytesWritten == data.size;
  }

protected:
  uint16_t bytesAvailable() override {
    if (stagingHead < stagingTail) {
      return stagingTail - stagingHead;
    }

    stagingHead = 0;
    stagingTail = 0;

    boost::asio::serial_port::native_handle_type handle =
        serialPort.native_handle();
    int bytesReady = 0;

#ifdef _WIN32
    COMSTAT comStat;
    DWORD errors;
    if (clearCommError(handle, &errors, &comStat)) {
      bytesReady = static_cast<int>(comStat.cbInQue);
    }
#else
    if (ioctl(handle, FIONREAD, &bytesReady) < 0) {
      bytesReady = 0;
    }
#endif
    if (bytesReady <= 0)
      return 0;

    const size_t toRead =
        std::min(static_cast<size_t>(bytesReady), sizeof(stagingBuffer));

    boost::system::error_code ec;
    const size_t bytesRead =
        serialPort.read_some(boost::asio::buffer(stagingBuffer, toRead), ec);

    if (ec || bytesRead == 0)
      return 0;

    stagingTail = static_cast<uint16_t>(bytesRead);
    return stagingTail;
  }

  uint8_t readByte() override { return stagingBuffer[stagingHead++]; }
};
#endif

#endif // SMARTDRIVE_PCSERIALTRANSPORT_H