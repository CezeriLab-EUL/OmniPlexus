//
// Created by dunamis on 29/04/2026.
//

#pragma once

class IConnectable {
public:
  virtual ~IConnectable() = default;
  virtual bool isConnected() const = 0;
};
