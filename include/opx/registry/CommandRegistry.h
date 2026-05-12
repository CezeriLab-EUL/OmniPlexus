//
// Created by dunamis on 16/02/2026.
//

#ifndef SMARTDRIVE_COMMANDREGISTRY_H
#define SMARTDRIVE_COMMANDREGISTRY_H

#include <cstdint>
#include "../core/ValueSource.h"

#ifndef EMBEDDED_BUILD
#include <map>
#include <string>
#include <vector>
#include <optional>

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
    void registerCommand(CommandMeta meta) {
        commands[meta.commandType] = meta;
    }

    std::optional<CommandMeta> getCommandInfo(uint16_t commandType) const {
        auto it = commands.find(commandType);
        if (it != commands.end()) return it->second;
        return std::nullopt;
    }

    std::optional<CommandMeta> findByName(const std::string& name) const {
        for (const auto& [id, meta]: commands) {
            if (meta.name == name) return meta;
        }
        return std::nullopt;
    }

    std::vector<const CommandMeta*> getAllCommands() const {
        std::vector<const CommandMeta*> result;
        result.reserve(commands.size());
        for (const auto& [id, meta]: commands) {
            result.push_back(&meta);
        }
        return result;
    }

    void initialize();
};
#endif

#endif //SMARTDRIVE_COMMANDREGISTRY_H