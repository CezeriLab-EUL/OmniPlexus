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
            param["description"].get<std::string>().empty()) {
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

            if (param.contains("required") && !param["required"].get<bool>() &&
                param.contains("default") && param["default"].is_string()) {
                const std::string defaultVal = param["default"].get<std::string>();
                const size_t maxLen = param.contains("maxLength") ? param["maxLength"].get<size_t>() : 0;
                if (defaultVal.length() > maxLen) {
                    result.addError(
                        "Command '" + cmdName + "' param '" + paramName +
                        "' has default value '" + defaultVal + "' that exceeds maxLength (" +
                        std::to_string(maxLen) + ")"
                    );
                }
            }
        }
    }

    static void validateParamOrdering(const json &params,
                                      const std::string &cmdName,
                                      ValidationResult &result) {
        bool foundOptional = false;
        int optionalCount = 0;

        for (size_t i = 0; i < params.size(); ++i) {
            const auto &param = params[i];

            if (!param.contains("required") || !param["required"].is_boolean()) {
                continue; // Already validated in validateParam
            }

            const bool isRequired = param["required"].get<bool>();
            const std::string paramName = param.contains("name") && param["name"].is_string()
                                              ? param["name"].get<std::string>()
                                              : "[unnamed]";

            if (!isRequired) {
                // Optional parameter
                optionalCount++;
                foundOptional = true;

                // Check if it's the last parameter
                if (i != params.size() - 1) {
                    result.addError(
                        "Command '" + cmdName + "' has optional parameter '" + paramName +
                        "' that is not the last parameter. Optional parameters must be last."
                    );
                }

                // Check if it has a default value
                if (!param.contains("default") || !param["default"].is_string() ||
                    param["default"].get<std::string>().empty()) {
                    result.addError(
                        "Command '" + cmdName + "' has optional parameter '" + paramName +
                        "' without a 'default' value. All optional parameters must have defaults."
                    );
                }
            } else {
                // Required parameter
                if (foundOptional) {
                    result.addError(
                        "Command '" + cmdName + "' has required parameter '" + paramName +
                        "' after an optional parameter. All required parameters must come before optional ones."
                    );
                }
            }
        }

        if (optionalCount > 1) {
            result.addError(
                "Command '" + cmdName + "' has " + std::to_string(optionalCount) +
                " optional parameters. Maximum is 1 optional parameter per command."
            );
        }
    }

    static bool isValidCommandName(const std::string &name) {
        if (name.empty()) return false;

        if (!std::isalpha(name[0]) && name[0] != '_') return false;

        for (char c: name) {
            if (!std::isalnum(c) && c != '_') return false;
        }

        return true;
    }

public:
    static ValidationResult validate(const json &data) {
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

        for (size_t i = 0; i < data["commands"].size(); ++i) {
            const auto &cmd = data["commands"][i];
            const std::string cmdLabel = "commands[" + std::to_string(i) + "]";

            if (!cmd.contains("name") || !cmd["name"].is_string()
                || cmd["name"].get<std::string>().empty()) {
                result.addError(cmdLabel + " is missing a valid 'name' field");
                continue;
            }

            const std::string name = cmd["name"].get<std::string>();

            if (!isValidCommandName(name)) {
                result.addError(
                    "Command '" + name + "' has invalid name format. "
                    "Names must start with a letter or underscore, and contain only "
                    "alphanumeric characters and underscores (no spaces or special characters)"
                );
            }

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
                result.addError(
                    cmdLabel + " has an invalid 'id' value (" + toHex(id) + "). Must be >= " + toHex(MIN_ID));
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
                for (size_t j = 0; j < cmd["params"].size(); ++j) {
                    validateParam(cmd["params"][j], name, static_cast<int>(j), result);
                }

                validateParamOrdering(cmd["params"], name, result);
            }

            if (prevID > 0 && id != prevID + 1) {
                result.addWarning("ID gap detected between " + toHex(prevID) +
                                  " and " + toHex(id) +
                                  " (this may be intentional for grouping)");
            }
            prevID = id;

            if (!cmd.contains("description") || !cmd["description"].is_string() || cmd["description"].get<std::string>()
                .empty()) {
                result.addWarning(cmdLabel + " is missing a 'description' field");
            }
        }

        return result;
    }
};
#endif //SMARTDRIVE_VALIDATOR_H
