//
// Created by dunamis on 30/04/2026.
//

#ifndef SMARTDRIVE_PCWIFITRANSPORT_H
#define SMARTDRIVE_PCWIFITRANSPORT_H

#ifndef ARDUINO
#include <boost/asio.hpp>
#include "opx/transport/AbstractTransport.h"
#include "opx/interfaces/IConnectable.h"
#include "opx/utils/Logger.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>

#ifndef PC_WIFI_ROLE_DEFINED
#define PC_WIFI_ROLE_DEFINED

enum class WifiRole : uint8_t {
    CLIENT,
    SERVER
};
#endif

class PcWiFiTransport : public AbstractTransport, public IConnectable {
public:
    using DisconnectCallback = void (*)(void *context);

private:
    uint8_t stagingBuffer[ProtocolConstants::MAX_FRAME_SIZE];
    uint16_t stagingHead = 0;
    uint16_t stagingTail = 0;

    WifiRole role;
    boost::asio::io_context ioContext;
    boost::asio::ip::tcp::socket socket;
    boost::asio::ip::tcp::acceptor *acceptor = nullptr;

    std::thread acceptThread;
    mutable std::mutex socketMutex;

    std::atomic<bool> clientConnected{false};

    std::string targetHost;
    uint16_t targetPort = 0;
    uint8_t maxReconnectAttempts;
    uint32_t reconnectDelayMs;
    uint8_t reconnectAttempts = 0;
    bool giveUp = false;
    std::chrono::steady_clock::time_point lastReconnectAttemptTime;

    DisconnectCallback disconnectCb = nullptr;
    void *disconnectCbContext = nullptr;

    void handleDisconnect() {
        clientConnected = false;
        LOG(LogLevel::OP_WARNING, "WiFi transport: client disconnected");
        {
            std::lock_guard<std::mutex> lock(socketMutex);
            boost::system::error_code ec;
            socket.close(ec);
        }
        if (disconnectCb) {
            disconnectCb(disconnectCbContext);
        }
        if (role == WifiRole::CLIENT) {
            LOG(LogLevel::OP_INFO, "WiFi transport: attempting reconnect...");
            if (tryConnect()) {
                LOG(LogLevel::OP_INFO, "WiFi transport: reconnected successfully");
            } else {
                LOG(LogLevel::OP_WARNING, "WiFi transport: reconnect failed, will retry next accumulate()");
            }
        }
    }

    bool tryConnect() {
        try {
            boost::asio::ip::tcp::resolver resolver(ioContext);
            auto endpoints = resolver.resolve(targetHost, std::to_string(targetPort));
            std::lock_guard<std::mutex> lock(socketMutex);
            if (socket.is_open()) {
                boost::system::error_code ec;
                socket.close(ec);
            }
            boost::asio::connect(socket, endpoints);
            clientConnected = true;
            reconnectAttempts = 0;
            giveUp = false;
            return true;
        } catch (const std::exception &e) {
            clientConnected = false;
            return false;
        }
    }

    void startAcceptLoop() {
        acceptThread = std::thread([this]() {
            while (true) {
                try {
                    boost::asio::ip::tcp::socket newSocket(ioContext);
                    acceptor->accept(newSocket);
                    {
                        std::lock_guard<std::mutex> lock(socketMutex);
                        if (socket.is_open()) {
                            boost::system::error_code ec;
                            socket.close(ec);
                        }
                        socket = std::move(newSocket);
                        clientConnected = true;
                    }
                    LOG(LogLevel::OP_INFO, "WiFi SERVER: client connected");
                } catch (const std::exception &e) {
                    // Acceptor was closed (destructor called) — exit cleanly
                    break;
                }
            }
        });
    }

public:
    explicit PcWiFiTransport(uint16_t port)
        : role(WifiRole::SERVER),
          socket(ioContext),
          maxReconnectAttempts(0),
          reconnectDelayMs(0) {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        acceptor = new boost::asio::ip::tcp::acceptor(ioContext, endpoint);
        LOG(LogLevel::OP_INFO, "WiFi SERVER: listening for connections");
        startAcceptLoop();
    }

    PcWiFiTransport(const char *host, uint16_t port,
                    uint8_t maxReconnectAttempts = 5,
                    uint32_t reconnectDelayMs = 2000)
        : role(WifiRole::CLIENT),
          socket(ioContext),
          targetHost(host),
          targetPort(port),
          maxReconnectAttempts(maxReconnectAttempts),
          reconnectDelayMs(reconnectDelayMs),
          lastReconnectAttemptTime(std::chrono::steady_clock::now()) {
        if (tryConnect()) {
            LOG(LogLevel::OP_INFO, "WiFi CLIENT: connected to server");
        } else {
            LOG(LogLevel::OP_WARNING, "WiFi CLIENT: initial connection failed, will retry in accumulate()");
        }
    }

    ~PcWiFiTransport() {
        clientConnected = false;
        {
            std::lock_guard<std::mutex> lock(socketMutex);
            if (socket.is_open()) {
                boost::system::error_code ec;
                socket.close(ec);
            }
        }
        if (acceptor) {
            boost::system::error_code ec;
            acceptor->close(ec); // causes accept() to throw, exiting the thread
            if (acceptThread.joinable()) {
                acceptThread.join();
            }
            delete acceptor;
            acceptor = nullptr;
        }
    }

    void onClientDisconnected(DisconnectCallback cb, void *context = nullptr) {
        disconnectCb = cb;
        disconnectCbContext = context;
    }

    bool isConnected() const override {
        return clientConnected.load();
    }

    bool send(const SerializedData &data) override {
        if (!clientConnected) {
            LOG(LogLevel::OP_WARNING, "WiFi transport: send() called but not connected");
            return false;
        }
        try {
            std::lock_guard<std::mutex> lock(socketMutex);
            const size_t bytesWritten = boost::asio::write(
                socket,
                boost::asio::buffer(data.data, data.size)
            );
            return bytesWritten == data.size;
        } catch (const std::exception &e) {
            LOG(LogLevel::OP_ERROR, "WiFi transport: send() failed");
            return false;
        }
    }


    void accumulate() override {
        if (role == WifiRole::SERVER && !clientConnected) {
            return;
        }

        if (role == WifiRole::CLIENT && !clientConnected) {
            if (giveUp) return;

            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastReconnectAttemptTime).count();

            if (elapsed < static_cast<long>(reconnectDelayMs)) return;

            lastReconnectAttemptTime = now;
            reconnectAttempts++;

            LOG(LogLevel::OP_WARNING, "WiFi CLIENT: reconnect attempt...");

            if (reconnectAttempts > maxReconnectAttempts) {
                LOG(LogLevel::OP_ERROR, "WiFi CLIENT: max reconnect attempts reached, giving up");
                giveUp = true;
                return;
            }

            if (tryConnect()) {
                LOG(LogLevel::OP_INFO, "WiFi CLIENT: reconnected successfully");
            } else {
                LOG(LogLevel::OP_WARNING, "WiFi CLIENT: reconnect attempt failed");
            }
            return;
        }

        bool disconnected = false;
        {
            std::lock_guard<std::mutex> lock(socketMutex);
            if (!socket.is_open()) {
                disconnected = true;
            } else {
                boost::system::error_code ec;
                socket.non_blocking(true, ec);
                uint8_t peek;
                socket.receive(
                    boost::asio::buffer(&peek, 1),
                    boost::asio::ip::tcp::socket::message_peek,
                    ec
                );
                socket.non_blocking(false, ec);

                if (ec == boost::asio::error::eof ||
                    ec == boost::asio::error::connection_reset) {
                    disconnected = true;
                }
            }
        } // ← mutex released here before handleDisconnect()

        if (disconnected) {
            handleDisconnect();
            return;
        }

        AbstractTransport::accumulate();
    }

protected:
    uint16_t bytesAvailable() override {
        // If staging buffer still has unread bytes, report those first
        if (stagingHead < stagingTail) {
            return stagingTail - stagingHead;
        }

        // Reset staging buffer
        stagingHead = 0;
        stagingTail = 0;

        boost::system::error_code ec;
        const size_t available = socket.available(ec);
        if (ec || available == 0) return 0;

        const size_t toRead = std::min(available, sizeof(stagingBuffer));

        const size_t bytesRead = socket.read_some(
            boost::asio::buffer(stagingBuffer, toRead), ec
        );

        if (ec || bytesRead == 0) return 0;

        stagingTail = static_cast<uint16_t>(bytesRead);
        return stagingTail;
    }

    uint8_t readByte() override {
        return stagingBuffer[stagingHead++];
    }
};

#endif // ARDUINO
#endif // SMARTDRIVE_PCWIFITRANSPORT_H
