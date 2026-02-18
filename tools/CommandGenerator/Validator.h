//
// Created by dunamis on 17/02/2026.
//

#ifndef SMARTDRIVE_VALIDATOR_H
#define SMARTDRIVE_VALIDATOR_H

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <iomanip>
#include "lib/nlohmann/json.hpp"

using json = nlohmann::json;

static const std::set<std::string> VALID_TYPES = {
    "UINT8", "INT8", "FLOAT", "UINT16", "INT16", "UINT32", "INT32", "STRING"
};

static const int MAX_PARAMS = 3;
static const uint16_t MIN_ID = 0x0001;
static const uint16_t MAX_ID = 0xFFFF;

struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    void addError(const std::string &error) {
        valid = false;
        errors.push_back(error);
    }

    void addWarning(const std::string &warning) {
        warnings.push_back(warning);
    }
};


class Validator {
private:
    static std::string toHex(uint16_t value) {
        std::ostringstream oss;
        oss << "0x" << std::uppercase << std::hex
                << std::setw(4) << std::setfill('0') << value;
        return oss.str();
    }

    static void validateParam(const json &param,
                              const std::string &cmdName,
                              int paramIndex,
                              ValidationResult &result) {

        if (!param.contains("name") || !param["name"].is_string()
            || param["name"].get<std::string>().empty()) {
            result.addError(
                "Command '" + cmdName + "' param[" + std::to_string(paramIndex) +
                "] is missing a valid 'name' field"
            );
        }

        const std::string paramName = param.contains("name") && param["name"].is_string()
        ? param["name"].get<std::string>()
        : "[unnamed]";

        if (!param.contains("type") || !param["type"].is_string()) {
            result.addError(
                "Command '" + cmdName + "' param '" + paramName +
                "' is missing the 'type' field"
            );
        } else {
            const std::string type = param["type"].get<std::string>();
            if (!VALID_TYPES.count(type)) {
                result.addError(
                    "Command '" + cmdName + "' param '" + paramName +
                    "' has unsupported type '" + type +
                    "'. Valid types are: UINT8, INT8, FLOAT, UINT16, INT16, UINT32, INT32, STRING"
                );
            }
        }

        if (!param.contains("required") || !param["required"].is_boolean()) {
            result.addError(
                "Command '" + cmdName + "' param '" + paramName +
                "' is missing the 'required' field (must be true or false)"
            );
        }

        if (!param.contains("description") ||
            !param["description"].is_string() ||
            param["description"].get<std::string>().empty()){
            result.addWarning(
                "Command '" + cmdName + "' param '" + paramName +
                "' is missing the 'description' field"
            );
        }

        if (param.contains("type") && param["type"].is_string() && param["type"].get<std::string>() == "STRING") {
            if (!param.contains("maxLength") || !param["maxLength"].is_number_integer()) {
                result.addError(
                    "Command '" + cmdName + "' param '" + paramName +
                    "' is missing the 'maxLength' field (must be a positive integer)"
                );
            }
        }
    }

public:
    static ValidationResult validate(const json& data) {
        ValidationResult result;

        if (!data.contains("commands")) {
            result.addError("JSON is missing the top-level 'commands' array");
            return result;
        }

        if (!data["commands"].is_array()) {
            result.addError("'commands' field is not an array");
            return result;
        }

        if (data["commands"].empty()) {
            result.addError("'commands' array is empty - nothing to generate");
            return result;
        }

        std::set<uint16_t> seenIDs;
        std::set<std::string> seenNames;
        uint16_t prevID = 0;

        for (size_t i=0; i<data["commands"].size(); ++i) {
            const auto& cmd = data["commands"][i];
            const std::string cmdLabel = "commands[" + std::to_string(i) + "]";

            if (!cmd.contains("name") || !cmd["name"].is_string()
                || cmd["name"].get<std::string>().empty()) {
                result.addError(cmdLabel + " is missing a valid 'name' field");
                continue;
            }

            const std::string name = cmd["name"].get<std::string>();

            if (!cmd.contains("id") || !cmd["id"].is_number_unsigned()) {
                result.addError(cmdLabel + " is missing a valid 'id' field");
                continue;
            }

            const uint16_t id = cmd["id"].get<uint16_t>();

            if (!cmd.contains("params") || !cmd["params"].is_array()) {
                result.addError(cmdLabel + " is missing a valid 'params' array");
                continue;
            }

            if (id < MIN_ID) {
                result.addError(cmdLabel + " has an invalid 'id' value (" + toHex(id) + "). Must be >= " + toHex(MIN_ID));
            }

            if (seenIDs.count(id)) {
                result.addError(cmdLabel + " has a duplicate 'id' value (" + toHex(id) + ")");
            }
            seenIDs.insert(id);

            if (seenNames.count(name)) {
                result.addError(cmdLabel + " has a duplicate 'name' value (" + name + ")");
            }
            seenNames.insert(name);

            if (cmd.contains("params") && cmd["params"].is_array()) {
                if (cmd["params"].size() > MAX_PARAMS) {
                    result.addError(cmdLabel + " has more than " + std::to_string(MAX_PARAMS) + " params");
                }
                for (size_t j=0; j<cmd["params"].size(); ++j) {
                    validateParam(cmd["params"][j], name, static_cast<int>(j), result);
                }
            }

            if (prevID > 0 && id != prevID + 1) {
                result.addWarning("ID gap detected between " + toHex(prevID) +
                    " and " + toHex(id) +
                    " (this may be intentional for grouping)");
            }
            prevID = id;

            if (!cmd.contains("description") || !cmd["description"].is_string() || cmd["description"].get<std::string>().empty()) {
                result.addWarning(cmdLabel + " is missing a 'description' field");
            }
        }

        return result;
    }
};
#endif //SMARTDRIVE_VALIDATOR_H
