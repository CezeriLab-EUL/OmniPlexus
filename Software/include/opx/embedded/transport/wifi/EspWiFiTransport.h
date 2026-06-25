//
// Created by dunamis on 29/04/2026.
//

#ifndef SMARTDRIVE_ESPWIFITRANSPORT_H
#define SMARTDRIVE_ESPWIFITRANSPORT_H

#include "opx/shared/core/Config.h" // IWYU pragma: keep

#if OPX_TARGET_ESP32
#include "opx/shared/interfaces/IConnectable.h"
#include "opx/shared/transport/AbstractTransport.h"
#include "opx/shared/utils/Logger.h"
#include <Arduino.h>
#include <WiFi.h>

#ifndef WIFI_ROLE_DEFINED
#define WIFI_ROLE_DEFINED
enum class WiFiRole : uint8_t { CLIENT, SERVER };
#endif

class EspWiFiTransport : public AbstractTransport, public IConnectable {
public:
  using DisconnectCallback = void (*)(void *context);

private:
  WiFiRole role;
  WiFiServer *server = nullptr;
  WiFiClient client;
  bool clientConnected = false;
  const char *targetHost = nullptr;
  uint16_t targetPort = 0;

  uint8_t maxReconnectAttempts;
  uint32_t reconnectDelayMs;
  uint8_t reconnectAttempts = 0;
  uint32_t lastReconnectAttemptMs = 0;
  bool giveUp = false;

  DisconnectCallback disconnectCb = nullptr;
  void *disconnectCbContext = nullptr;

  void handleDisconnect() {
    clientConnected = false;
    LOG(LogLevel::OP_WARNING, "WiFi transport: Client disconnected");
    if (disconnectCb) {
      disconnectCb(disconnectCbContext);
    }
    if (role == WiFiRole::CLIENT) {
      LOG(LogLevel::OP_INFO, "WiFi transport: attempting reconnect...");
      client = WiFiClient(); // reset the client object
      if (client.connect(targetHost, targetPort)) {
        clientConnected = true;
        LOG(LogLevel::OP_INFO, "WiFi transport: reconnected successfully");
      } else {
        LOG(LogLevel::OP_WARNING,
            "WiFi transport: reconnect failed, will retry next accumulate()");
      }
    }
  }

public:
  explicit EspWiFiTransport(uint16_t port) : role(WiFiRole::SERVER) {
    server = new WiFiServer(port);
    server->begin();
    LOG(LogLevel::OP_INFO,
        "WiFi transport: server started and listening for connections");
  }

  EspWiFiTransport(const char *host, uint16_t port,
                   uint8_t maxReconnectAttempts = 5,
                   uint32_t reconnectDelayMs = 2000)
      : role(WiFiRole::CLIENT), targetHost(host), targetPort(port),
        maxReconnectAttempts(maxReconnectAttempts),
        reconnectDelayMs(reconnectDelayMs) {
    if (client.connect(targetHost, targetPort)) {
      clientConnected = true;
      reconnectAttempts = 0;
      LOG(LogLevel::OP_INFO, "WiFi transport: connected to the server");
    } else {
      LOG(LogLevel::OP_ERROR,
          "WiFi transport: failed to connect to the server. Will retry");
    }
  }

  ~EspWiFiTransport() {
    if (client) {
      client.stop();
    }
    if (server) {
      server->stop();
      delete server;
      server = nullptr;
    }
  }

  void onClientDisconnected(DisconnectCallback cb, void *context = nullptr) {
    disconnectCb = cb;
    disconnectCbContext = context;
  }

  bool isConnected() const override { return clientConnected; }

  bool send(const SerializedData &data) override {
    if (!clientConnected) {
      LOG(LogLevel::OP_WARNING,
          "WiFi transport: no client connected, cannot send data");
      return false;
    }

    const size_t bytesWritten = client.write(data.data, data.size);

    if (bytesWritten != data.size) {
      LOG(LogLevel::OP_ERROR, "WiFi transport: failed to send all data");
      return false;
    }

    return true;
  }

  void accumulate() override {
    if (role == WiFiRole::SERVER && !clientConnected) {
      WiFiClient incoming = server->available();
      if (incoming) {
        client = incoming;
        clientConnected = true;
        LOG(LogLevel::OP_INFO, "WiFi transport: client connected");
      }
      return;
    }

    if (role == WiFiRole::CLIENT && !clientConnected) {
      if (giveUp)
        return;

      const uint32_t now = millis();
      if (now - lastReconnectAttemptMs < reconnectDelayMs)
        return;

      lastReconnectAttemptMs = now;
      reconnectAttempts++;

      LOG(LogLevel::OP_WARNING,
          "WiFi transport: attempting reconnect to the server...");

      if (reconnectAttempts > maxReconnectAttempts) {
        giveUp = true;
        LOG(LogLevel::OP_ERROR,
            "WiFi transport: failed to reconnect, giving up");
        return;
      }

      client = WiFiClient();
      if (client.connect(targetHost, targetPort)) {
        clientConnected = true;
        reconnectAttempts = 0; // reset on successful reconnect
        giveUp = false;
        LOG(LogLevel::OP_INFO, "WiFi transport: reconnected successfully");
      } else {
        LOG(LogLevel::OP_WARNING, "WiFi transport: reconnect attempt failed");
      }

      return;
    }

    if (!client.connected()) {
      handleDisconnect();
      return;
    }

    AbstractTransport::accumulate();
  }

protected:
  uint16_t bytesAvailable() override {
    const int available = client.available();
    return (available > 0) ? static_cast<uint16_t>(available) : 0;
  }

  uint8_t readByte() override {
    const int byte = client.read();
    return (byte >= 0) ? static_cast<uint8_t>(byte) : 0;
  }
};

#endif
#endif // SMARTDRIVE_ESPWIFITRANSPORT_H
