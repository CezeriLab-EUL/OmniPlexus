//
// Created by dunamis on 23/04/2026.
//

#ifndef SMARTDRIVE_ESPHTTPTRANSPORT_H
#define SMARTDRIVE_ESPHTTPTRANSPORT_H

#include "opx/shared/core/Config.h"

#if OPX_TARGET_ESP32
#include "opx/shared/constants/ProtocolConstants.h"
#include "opx/shared/interfaces/ITransport.h"
#include "opx/shared/types/ProtocolTypes.h"
#include "opx/shared/utils/Logger.h"
#include "types.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>

static constexpr const char *HTTP_ENDPOINT = "/opx]";

class EspHttpTransport;
static void httpServerTask(void *param);

class EspHttpTransport : public ITransport {
  struct FrameSlot {
    uint8_t buffer[ProtocolConstants::MAX_FRAME_SIZE];
    size_t size = 0;
    bool ready = false;
    SemaphoreHandle_t mutex = nullptr;

    void init() { mutex = xSemaphoreCreateMutex(); }

    struct Lock {
      const SemaphoreHandle_t &handle;
      bool taken;
      explicit Lock(const SemaphoreHandle_t &h) : handle(h), taken(false) {
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

  WebServer *server = nullptr;
  uint16_t listenPort = 0;

  uint16_t targetPort = 0;
  String targetHost;

public:
  explicit EspHttpTransport(uint16_t port)
      : role(HttpRole::SERVER), listenPort(port) {
    slot.init();

    server = new WebServer(listenPort);

    server->on(
        HTTP_ENDPOINT, HTTP_POST, [this]() { handlePostRequest(); },
        [this]() {
          // called as raw bytes arrive
          HTTPRaw &raw = server->raw();
          if (raw.status == RAW_START) {
            FrameSlot::Lock lock(slot.mutex);
            if (lock.taken)
              slot.size = 0;
          } else if (raw.status == RAW_WRITE) {
            FrameSlot::Lock lock(slot.mutex);
            if (lock.taken && (slot.size + raw.currentSize) <=
                                  ProtocolConstants::MAX_FRAME_SIZE) {
              memcpy(slot.buffer + slot.size, raw.buf, raw.currentSize);
              slot.size += raw.currentSize;
            }
          } else if (raw.status == RAW_END) {
            FrameSlot::Lock lock(slot.mutex);
            if (lock.taken && slot.size > 0) {
              slot.ready = true;
            }
          }
        });

    const char *headerKeys[] = {"Content-Length"};
    size_t headerKeysCount = sizeof(headerKeys) / sizeof(char *);
    server->collectHeaders(headerKeys, headerKeysCount);
    server->begin();
  }

  EspHttpTransport(const char *host, uint16_t port)
      : role(HttpRole::CLIENT), targetHost(host), targetPort(port) {
    slot.init();
  }

  ~EspHttpTransport() {
    if (role == HttpRole::SERVER) {
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

  bool send(const SerializedData &data) override {
    if (role == HttpRole::CLIENT) {
      HTTPClient http;

      // Build the full URL: http://host:port/opx
      String url = String("http://") + targetHost + ":" + String(targetPort) +
                   HTTP_ENDPOINT;

      http.begin(url);
      http.addHeader("Content-Type", "application/octet-stream");
      const int httpCode = http.POST(const_cast<uint8_t *>(data.data),
                                     static_cast<int>(data.size));

      if (httpCode <= 0) {
        LOG(LogLevel::OP_ERROR, "HTTP CLIENT: POST failed");
        http.end();
        return false;
      }

      if (httpCode != HTTP_CODE_OK) {
        LOG(LogLevel::OP_WARNING,
            "HTTP CLIENT: server returned non-200 status");
        http.end();
        return false;
      }

      // If the server sent data back, store it as an incoming frame.
      const int payloadSize = http.getSize();
      if (payloadSize > 0 && static_cast<size_t>(payloadSize) <=
                                 ProtocolConstants::MAX_FRAME_SIZE) {
        WiFiClient *stream = http.getStreamPtr();
        if (stream) {
          FrameSlot::Lock lock(slot.mutex);
          if (lock.taken && !slot.ready) {
            const size_t bytesRead = stream->readBytes(
                slot.buffer, static_cast<size_t>(payloadSize));
            if (bytesRead > 0) {
              slot.size = bytesRead;
              slot.ready = true;
            }
          }
        }
      }

      http.end();
      return true;
    }

    LOG(LogLevel::OP_WARNING,
        "HTTP SERVER: send() not supported in server role");
    return false;
  }

  void accumulate() override {
    if (server) {
      const unsigned long start = millis();
      while (millis() - start < 20) { // pump for 20ms
        server->handleClient();
      }
    }
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
    String contentLengthStr = server->header("Content-Length");
    Serial.print("Expected: ");
    Serial.print(contentLengthStr);
    Serial.print(" Got: ");
    Serial.println(slot.size);
    if (contentLengthStr.length() != 0) {
      int expectedSize = contentLengthStr.toInt();
      Serial.println(expectedSize);
      if (slot.size == 0 || slot.size != expectedSize) {
        LOG(LogLevel::OP_WARNING,
            "HTTP SERVER: received POST with unexpected size");
        server->sendHeader("Connection", "close");
        server->sendHeader("Content-Length", "0");
        server->send(400);
        return;
      }
      server->sendHeader("Connection", "close");
      server->sendHeader("Content-Length", "0");
      server->send(200);
    } else {
      LOG(LogLevel::OP_WARNING,
          "HTTP SERVER: received POST without Content-Length");
      server->sendHeader("Connection", "close");
      server->sendHeader("Content-Length", "0");
      server->send(400);
    }
  }
};

#endif

#endif // SMARTDRIVE_ESPHTTPTRANSPORT_H