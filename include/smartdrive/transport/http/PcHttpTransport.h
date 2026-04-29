//
// Created by dunamis on 23/04/2026.
//

#ifndef SMARTDRIVE_PCHTTPTRANSPORT_H
#define SMARTDRIVE_PCHTTPTRANSPORT_H

#ifndef ARDUINO

#include "external/httplib/httplib.h"
#include "smartdrive/interfaces/ITransport.h"
#include "smartdrive/types/ProtocolTypes.h"
#include "smartdrive/utils/Logger.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>
#include "types.h"

class PcHttpTransport : public ITransport {
    struct FrameSlot {
        uint8_t buffer[ProtocolConstants::MAX_FRAME_SIZE];
        size_t size = 0;
        bool ready = false;
        mutable std::mutex mtx;
    };

    HttpRole role;
    FrameSlot slot;

    // SERVER-only members
    httplib::Server svr;
    std::thread serverThread;
    std::atomic<bool> serverRunning{false};
    uint16_t listenPort = 0;

    //CLIENT-only members
    httplib::Client *cli = nullptr;
    std::string targetPath = "/smartdrive"; //POST endpoint

public:
    //server runs on a background thread so main loop can remain free
    explicit PcHttpTransport(uint16_t port) : role(HttpRole::SERVER), listenPort(port) {
        svr.Post(targetPath.c_str(), [this](const httplib::Request &req, httplib::Response &res) {
            if (req.body.empty()) {
                res.status = 400;
                return;
            }

            const size_t incoming = req.body.size();
            if (incoming > ProtocolConstants::MAX_FRAME_SIZE) {
                LOG(LogLevel::OP_WARNING, "HTTP SERVER: incoming frame too large, dropping");
                res.status = 413;
                return;
            }

            {
                std::lock_guard<std::mutex> lock(slot.mtx);
                if (slot.ready) { //prevous frame has not been consumed yet
                    LOG(LogLevel::OP_WARNING, "HTTP SERVER: previous frame not consumed, dropping new frame");
                    res.status = 503;
                    return;
                }

                memcpy(slot.buffer, req.body.data(), incoming);
                slot.size = incoming;
                slot.ready = true;
            }

            res.status = 200;
        });

        serverRunning = true;
        serverThread = std::thread([this]() {
            svr.listen("0.0.0.0", listenPort);
            serverRunning = false;
        });
    }

    PcHttpTransport(const char *host, uint16_t port) : role(HttpRole::CLIENT) {
        cli = new httplib::Client(host, port);
        cli->set_connection_timeout(2); //2 seconds
        cli->set_read_timeout(10, 0); //2 seconds
        cli->set_write_timeout(2); //2 seconds
        cli->set_keep_alive(false);
    }

    ~PcHttpTransport() {
        if (role == HttpRole::SERVER) {
            svr.stop();
            if (serverThread.joinable()) {
                serverThread.join();
            }
        } else if (role == HttpRole::CLIENT) {
            delete cli;
            cli = nullptr;
        }
    }

    bool send(const SerializedData &data) override {
        if (role == HttpRole::CLIENT) {
            if (!cli) {
                LOG(LogLevel::OP_ERROR, "HTTP CLIENT: client not initialized");
                return false;
            }

            auto res = cli->Post(targetPath.c_str(), reinterpret_cast<const char *>(data.data), data.size,
                                 "application/octet-stream");
            if (!res) {
                LOG(LogLevel::OP_ERROR, httplib::to_string(res.error()).c_str());
                LOG(LogLevel::OP_ERROR, "HTTP CLIENT: request failed (no response/timeout)");
                return false;
            }
            if (res->status != 200) {
                LOG(LogLevel::OP_ERROR,
                    ("HTTP CLIENT: server responded with status " + std::to_string(res->status)).c_str());
                return false;
            }
            if (!res->body.empty()) {
                const size_t bodySize = res->body.size();
                if (bodySize > ProtocolConstants::MAX_FRAME_SIZE) {
                    LOG(LogLevel::OP_WARNING, "HTTP CLIENT: response frame too large, ignoring body");
                    return true; //We consider this a success since the request itself succeeded
                }
                std::lock_guard<std::mutex> lock(slot.mtx);
                if (!slot.ready) {
                    memcpy(slot.buffer, res->body.data(), bodySize);
                    slot.size = bodySize;
                    slot.ready = true;
                }
            }
            return true;
        }

        LOG(LogLevel::OP_WARNING, "HTTP SERVER: send() called but transport is in SERVER mode, ignoring");
        return false;
    }

    void accumulate() override {
        //intentionally empty because http doesn't operate by streaming bytes
    }

    bool hasCompleteFrame() const override {
        std::lock_guard<std::mutex> lock(slot.mtx);
        return slot.ready;
    }

    RawData getFrame() override {
        RawData result;
        result.data = slot.buffer;
        result.size = slot.size;
        return result;
    }

    void releaseFrame() override {
        std::lock_guard<std::mutex> lock(slot.mtx);
        slot.ready = false;
        slot.size = 0;
    }
};
#endif

#endif //SMARTDRIVE_PCHTTPTRANSPORT_H
