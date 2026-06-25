//
// Created by dunamis on 16/02/2026.
//

#pragma once

#include "opx/shared/core/Config.h" // IWYU pragma: keep
#include "opx/shared/core/ValueSource.h"
#include <cstdint>

#ifndef OPX_TARGET_EMBEDDED
#include <map>
#include <optional>
#include <string>
#include <vector>

struct ParamInfo {
  std::string name;
  ValueType type;
  bool required;
  std::string description;
  size_t maxLength = 0;
  std::string defaultValue;
};

struct CommandMeta {
  uint16_t commandType;
  std::string name;
  std::string description;
  std::vector<ParamInfo> params;
};

class CommandRegistry {
private:
  std::map<uint16_t, CommandMeta> commands;

public:
  void registerCommand(CommandMeta meta) { commands[meta.commandType] = meta; }

  std::optional<CommandMeta> getCommandInfo(uint16_t commandType) const {
    auto it = commands.find(commandType);
    if (it != commands.end())
      return it->second;
    return std::nullopt;
  }

  std::optional<CommandMeta> findByName(const std::string &name) const {
    for (const auto &[id, meta] : commands) {
      if (meta.name == name)
        return meta;
    }
    return std::nullopt;
  }

  std::vector<const CommandMeta *> getAllCommands() const {
    std::vector<const CommandMeta *> result;
    result.reserve(commands.size());
    for (const auto &[id, meta] : commands) {
      result.push_back(&meta);
    }
    return result;
  }

  void initialize();

  const CommandMeta *getMeta(uint16_t commandType) const {
    auto it = commands.find(commandType);
    if (it != commands.end())
      return &it->second;
    return nullptr;
  }
};
#endif