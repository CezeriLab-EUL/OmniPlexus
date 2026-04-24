//
// Created by dunamis on 23/04/2026.
//

#ifndef SMARTDRIVE_ESPHTTPTRANSPORT_H
#define SMARTDRIVE_ESPHTTPTRANSPORT_H

#ifdef ARDUINO

#include <Arduino.h>
#include <WiFi.h>
#include "smartdrive/interfaces/ITransport.h"
#include "smartdrive/types/ProtocolTypes.h"
#include "smartdrive/utils/Logger.h"
#include "types.h"

// 4096 bytes is comfortable for httplib's internal parsing + your lambda.
// If you add heavy processing inside the handler, increase this.
static constexpr uint32_t HTTP_TASK_STACK_SIZE  = 4096;
static constexpr UBaseType_t HTTP_TASK_PRIORITY = 1;
static constexpr BaseType_t  HTTP_TASK_CORE     = 0;  // pin to core 0, leave core 1 for the main loop
static constexpr const char* HTTP_ENDPOINT = "/smartdrive";

class EspHttpTransport;
static void httpServerTask(void* param);

class EspHttpTransport : public ITransport {
    struct FrameSlot {
        uint8_t buffer[ProtocolConstants::MAX_FRAME_SIZE];
        size_t size = 0;
        bool ready = false;
        SemaphoreHandle_t mutex = nullptr;

        void init() {
            mutex = xSemaphoreCreateMutex();
        }

        struct Lock {
            SemaphoreHandle_t& handle;
            bool taken;
            explicit Lock(SemaphoreHandle_t& h) : handle(h), taken(false) {
                taken = (xSemaphoreTake(handle, portMAX_DELAY) == pdTRUE);
            }
            ~Lock() {
                if (taken) {
                    xSemaphoreGive(handle);
                }
            }
        };
    };

    HttpRole role;
    FrameSlot slot;

    WebServer* server = nullptr;
    TaskHandle_t serverTask = nullptr;
    uint16_t listenPort = 0;

    uint16_t targetPort = 0;
    String targetHost;

public:
    explicit EspHttpTransport(uint16_t port) : role(HttpRole::SERVER), listenPort(port) {
        slot.init();

        server = new WebServer(listenPort);

        server->on(HTTP_ENDPOINT, HTTP_POST, [this]() {
            handlePostRequest();
        });

        server->begin();

        xTaskCreatePinnedToCore(httpServerTask,
            "OmniPlexusHTTP",
            HTTP_TASK_STACK_SIZE,
            this,
            HTTP_TASK_PRIORITY, &serverTask, HTTP_TASK_CORE);
    }

    EspHttpTransport(const char* host, uint16_t port) : role(HttpRole::CLIENT), targetHost(host), targetPort(port) {
        slot.init();
    }

    ~EspHttpTransport() {
        if (role == HttpRole::SERVER) {
            if (serverTask) {
                vTaskDelete(serverTask);
                serverTask = nullptr;
            }
            if (server) {
                server->stop();
                delete server;
                server = nullptr;
            }
        }

        if (slot.mutex) {
            vSemaphoreDelete(slot.mutex);
            slot.mutex = nullptr;
        }
    }

    bool send(const SerializedData& data) override {
        if (role == HttpRole::CLIENT) {
            HTTPClient http;

            // Build the full URL: http://host:port/smartdrive
            String url = String("http://") + targetHost + ":" +
                         String(targetPort) + HTTP_ENDPOINT;

            http.begin(url);
            http.addHeader("Content-Type", "application/octet-stream");
            .
            const int httpCode = http.POST(
                const_cast<uint8_t*>(data.data),
                static_cast<int>(data.size)
            );

            if (httpCode <= 0) {
                LOG(LogLevel::OP_ERROR, "HTTP CLIENT: POST failed");
                http.end();
                return false;
            }

            if (httpCode != HTTP_CODE_OK) {
                LOG(LogLevel::OP_WARNING, "HTTP CLIENT: server returned non-200 status");
                http.end();
                return false;
            }

            // If the server sent data back, store it as an incoming frame.
            const int payloadSize = http.getSize();
            if (payloadSize > 0 &&
                static_cast<size_t>(payloadSize) <= ProtocolConstants::MAX_FRAME_SIZE)
            {
                WiFiClient* stream = http.getStreamPtr();
                if (stream) {
                    FrameSlot::Lock lock(slot.mutex);
                    if (lock.taken && !slot.ready) {
                        const size_t bytesRead = stream->readBytes(
                            slot.buffer,
                            static_cast<size_t>(payloadSize)
                        );
                        if (bytesRead > 0) {
                            slot.size  = bytesRead;
                            slot.ready = true;
                        }
                    }
                }
            }

            http.end();
            return true;
        }

        LOG(LogLevel::OP_WARNING, "HTTP SERVER: send() not supported in server role");
        return false;
    }

    void accumulate() override {
        //intentially empty
    }

    bool hasCompleteFrame() const override {
        FrameSlot::Lock lock(slot.mutex);
        return lock.taken && slot.ready;
    }

    RawData getFrame() override {
        RawData result;
        result.data = slot.buffer;
        result.size = slot.size;
        return result;
    }

    void releaseFrame() override {
        FrameSlot::Lock lock(slot.mutex);
        if (lock.taken) {
            slot.ready = false;
            slot.size = 0;
        }
    }

private:
    void handlePostRequest() {
        if (!server->hasArg("plain") || server->arg("plain").length() == 0) {
            server->send(400);
            return;
        }

        const String& body    = server->arg("plain");
        const size_t  bodyLen = body.length();

        if (bodyLen > ProtocolConstants::MAX_FRAME_SIZE) {
            LOG(LogLevel::OP_WARNING, "HTTP SERVER: incoming frame too large, dropping");
            server->send(413);
            return;
        }

        {
            FrameSlot::Lock lock(slot.mutex);

            if (!lock.taken) {
                server->send(503);
                return;
            }

            if (slot.ready) {
                // Previous frame not yet consumed — drop incoming, same
                // policy as AbstractTransport and PcHttpTransport.
                LOG(LogLevel::OP_WARNING, "HTTP SERVER: previous frame not released, dropping");
                server->send(503);
                return;
            }

            memcpy(slot.buffer, body.c_str(), bodyLen);
            slot.size  = bodyLen;
            slot.ready = true;
        }

        server->send(200);
    }

    friend void httpServerTask(void* param);
};


static void httpServerTask(void* param) {
    EspHttpTransport* self = static_cast<EspHttpTransport*>(param);
    for (;;) {
        if (self->server) {
            self->server->handleClient();
        }
        vTaskDelay(1);  // yield for 1 FreeRTOS tick (~1ms at default tick rate)
    }
}

#endif

#endif //SMARTDRIVE_ESPHTTPTRANSPORT_H