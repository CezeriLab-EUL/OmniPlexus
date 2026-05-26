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
static const int MAX_OPTIONAL_PARAMS = 1;
static const uint16_t MIN_ID = 0x0001;
static const uint16_t MAX_ID = 0xFFFF;
static const uint8_t CATEGORY_COMMAND = 0x0;
static const uint8_t CATEGORY_TELEMETRY = 0x1;
static const uint8_t CATEGORY_SETTING_GET = 0x2;
static const uint8_t CATEGORY_SETTING_SET = 0x3;

static uint16_t buildCommandID(uint16_t typeShift, uint8_t category, uint16_t localID) {
    return static_cast<uint16_t>((typeShift << 11) | (category << 8) | localID);
}

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
            if (!param.contains("maxLength") || !param["maxLength"].is_number_unsigned()) {
                result.addError(
                    "Command '" + cmdName + "' param '" + paramName +
                    "' is missing a valid 'maxLength' field (must be a positive integer)"
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

        if (optionalCount > MAX_OPTIONAL_PARAMS) {
            result.addError(
                "Command '" + cmdName + "' has " + std::to_string(optionalCount) +
                " optional parameters. Maximum is " + std::to_string(MAX_OPTIONAL_PARAMS) +
                " optional parameter per command."
            );
        }
    }

    static bool isValidCommandName(const std::string &name) {
        if (!std::isalpha(name[0]) && name[0] != '_') return false;

        for (char c: name) {
            if (!std::isalnum(c) && c != '_') return false;
        }

        return true;
    }

    static void validateTelemetrySource(const json &source, int index, ValidationResult &result) {
        const std::string label = "telemetry[" + std::to_string(index) + "]";
        if (!source.contains("name") || !source["name"].is_string() || source["name"].get<std::string>().empty()) {
            result.addError(label + " is missing a valid 'name' field");
        } else {
            const std::string name = source["name"].get<std::string>();
            if (!isValidCommandName(name)) {
                result.addError(
                    "Telemetry source '" + name + "' has invalid name format. "
                    "Names must start with a letter or underscore, and contain only "
                    "alphanumeric characters and underscores (no spaces or special characters)"
                );
            }
        }

        if (!source.contains("id") || !source["id"].is_number_unsigned()) {
            result.addError(label + " is missing a valid 'id' field");
        } else {
            const uint16_t id = source["id"].get<uint16_t>();
            if (id < MIN_ID) {
                result.addError(
                    label + " has an invalid 'id' value (" + toHex(id) + "). Must be >= " + toHex(MIN_ID));
            }
        }

        if (!source.contains("type") || !source["type"].is_string()) {
            result.addError(label + " is missing the 'type' field");
        } else {
            const std::string type = source["type"].get<std::string>();
            if (!VALID_TYPES.count(type)) {
                result.addError(
                    label + " has unsupported type '" + type +
                    "'. Valid types are: UINT8, INT8, FLOAT, UINT16, INT16, UINT32, INT32, STRING"
                );
            }
        }

        if (!source.contains("description") || !source["description"].is_string() || source["description"].get<
                std::string>().empty()) {
            result.addWarning(label + " is missing a 'description' field");
        }
    }

    static void validateTelemetry(const json &data, ValidationResult &result) {
        if (!data.contains("telemetry")) return; // telemetry is optional

        if (!data["telemetry"].is_array()) {
            result.addError("'telemetry' field must be an array");
            return;
        }

        if (data["telemetry"].empty()) {
            result.addWarning("'telemetry' array is empty — nothing to generate");
            return;
        }

        std::set<uint16_t> seenIDs;
        std::set<std::string> seenNames;
        uint16_t prevID = 0;

        for (size_t i = 0; i < data["telemetry"].size(); ++i) {
            const auto &source = data["telemetry"][i];

            validateTelemetrySource(source, static_cast<int>(i), result);

            // Duplicate ID check
            if (source.contains("id") && source["id"].is_number_unsigned()) {
                const uint16_t id = source["id"].get<uint16_t>();
                if (seenIDs.count(id)) {
                    result.addError(
                        "telemetry[" + std::to_string(i) + "] has duplicate 'id' value ("
                        + toHex(id) + ")"
                    );
                }
                seenIDs.insert(id);

                if (prevID > 0 && id != prevID + 1) {
                    result.addWarning(
                        "Telemetry ID gap detected between " + toHex(prevID) +
                        " and " + toHex(id) + " (this may be intentional)"
                    );
                }
                prevID = id;
            }

            if (source.contains("name") && source["name"].is_string()) {
                const std::string name = source["name"].get<std::string>();
                if (seenNames.count(name)) {
                    result.addError(
                        "telemetry[" + std::to_string(i) + "] has duplicate 'name' value ("
                        + name + ")"
                    );
                }
                seenNames.insert(name);
            }
        }
    }

    static void validateSettings(const json &data, ValidationResult &result) {
    if (!data.contains("settings")) return; // settings is optional

    if (!data["settings"].is_array()) {
        result.addError("'settings' field must be an array");
        return;
    }

    if (data["settings"].empty()) {
        result.addWarning("'settings' array is empty — nothing to generate");
        return;
    }

    std::set<uint16_t> seenIDs;
    std::set<std::string> seenNames;
    uint16_t prevID = 0;

    for (size_t i = 0; i < data["settings"].size(); ++i) {
        const auto &setting = data["settings"][i];
        const std::string label = "settings[" + std::to_string(i) + "]";

        if (!setting.contains("name") || !setting["name"].is_string() ||
            setting["name"].get<std::string>().empty()) {
            result.addError(label + " is missing a valid 'name' field");
        } else {
            const std::string name = setting["name"].get<std::string>();
            if (!isValidCommandName(name)) {
                result.addError(
                    "Setting '" + name + "' has invalid name format. "
                    "Names must start with a letter or underscore, and contain only "
                    "alphanumeric characters and underscores"
                );
            }
            if (seenNames.count(name)) {
                result.addError(label + " has duplicate 'name' value (" + name + ")");
            }
            seenNames.insert(name);
        }

        if (!setting.contains("id") || !setting["id"].is_number_unsigned()) {
            result.addError(label + " is missing a valid 'id' field");
        } else {
            const uint16_t id = setting["id"].get<uint16_t>();
            if (id < MIN_ID) {
                result.addError(
                    label + " has invalid 'id' value (" + toHex(id) +
                    "). Must be >= " + toHex(MIN_ID)
                );
            }
            if (seenIDs.count(id)) {
                result.addError(label + " has duplicate 'id' value (" + toHex(id) + ")");
            }
            seenIDs.insert(id);

            if (prevID > 0 && id != prevID + 1) {
                result.addWarning(
                    "Settings ID gap detected between " + toHex(prevID) +
                    " and " + toHex(id) + " (this may be intentional)"
                );
            }
            prevID = id;
        }

        if (!setting.contains("type") || !setting["type"].is_string()) {
            result.addError(label + " is missing the 'type' field");
        } else {
            const std::string type = setting["type"].get<std::string>();
            if (!VALID_TYPES.count(type)) {
                result.addError(
                    label + " has unsupported type '" + type + "'. "
                    "Valid types are: UINT8, INT8, FLOAT, UINT16, INT16, UINT32, INT32, STRING"
                );
            }
        }

        if (!setting.contains("description") || !setting["description"].is_string() ||
            setting["description"].get<std::string>().empty()) {
            result.addWarning(label + " is missing a 'description' field");
        }
    }
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

        if (!data.contains("device") || !data["device"].is_string() || data["device"].get<std::string>().empty()) {
            result.addError("JSON is missing the top-level 'device' field");
            return result;
        }

        const std::string deviceName = data["device"].get<std::string>();
        if (!std::isupper(deviceName[0])) {
            result.addError("Device name '" + deviceName + "' should start with an uppercase letter (PascalCase)");
        }
        for (const char c: deviceName) {
            if (!std::isalnum(c)) {
                result.addError("'device' field must contain only alphanumeric characters (no spaces or underscores)");
                break;
            }
        }

        if (!data.contains("typeShift") || !data["typeShift"].is_number_unsigned()) {
            result.addError("'typeShift' field is missing. 'typeShift' field must be an unsigned integer");
        } else {
            const uint16_t typeShift = data["typeShift"].get<uint16_t>();
            if (typeShift > 0x1F) {
                result.addError("'typeShift' field must be a value between 0 and 31");
            }
        }

        std::set<uint16_t> seenIDs;
        std::set<std::string> seenNames;

        for (size_t i = 0; i < data["commands"].size(); ++i) {
            const auto &cmd = data["commands"][i];
            const std::string cmdLabel = "commands[" + std::to_string(i) + "]";

            if (!cmd.contains("name") || !cmd["name"].is_string()
                || cmd["name"].get<std::string>().empty()) {
                result.addError(cmdLabel + " is missing a valid 'name' field");
                continue;
            }

            if (!cmd.contains("acknowledges") || !cmd["acknowledges"].is_boolean()) {
                result.addError(cmdLabel + " is missing a valid 'acknowledges' field");
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

            if (!cmd.contains("description") || !cmd["description"].is_string() || cmd["description"].get<std::string>()
                .empty()) {
                result.addWarning(cmdLabel + " is missing a 'description' field");
            }
        }

        validateTelemetry(data, result);
        validateSettings(data, result);

        return result;
    }

    static ValidationResult validateCrossDevice(const std::vector<json> &allData) {
        ValidationResult result;
        // Maps shifted command ID -> "DeviceName::COMMAND_NAME"
        std::map<uint16_t, std::string> seenCommandIDs;
        // Maps shifted telemetry ID -> "DeviceName::TELEMETRY_NAME"
        std::map<uint16_t, std::string> seenTelemetryIDs;
        std::map<uint16_t, std::string> seenShifts;

        for (const auto &data: allData) {
            if (!data.contains("device") || !data.contains("typeShift")) continue;
            const std::string deviceName = data["device"].get<std::string>();
            const uint16_t shift = static_cast<uint16_t>(data["typeShift"].get<uint32_t>());
            auto it = seenShifts.find(shift);
            if (it != seenShifts.end()) {
                result.addError(
                    "Duplicate 'typeShift' value " + toHex(shift) +
                    ": device '" + deviceName + "' conflicts with '" + it->second + "'"
                );
            } else {
                seenShifts[shift] = deviceName;
            }
        }

        for (const auto &data: allData) {
            if (!data.contains("device") || !data.contains("typeShift")) continue;
            const std::string deviceName = data["device"].get<std::string>();
            const uint16_t shift = static_cast<uint16_t>(data["typeShift"].get<uint32_t>());

            if (data.contains("commands") && data["commands"].is_array()) {
                for (const auto &cmd: data["commands"]) {
                    if (!cmd.contains("id") || !cmd.contains("name")) continue;
                    const uint16_t rawId = cmd["id"].get<uint16_t>();
                    const uint16_t builtID = buildCommandID(shift, CATEGORY_COMMAND, rawId);
                    const std::string label = deviceName + "::" + cmd["name"].get<std::string>();

                    auto it = seenCommandIDs.find(builtID);
                    if (it != seenCommandIDs.end()) {
                        result.addError(
                            "Cross-device command ID collision at shifted ID " + toHex(builtID) +
                            ": '" + label + "' conflicts with '" + it->second + "'"
                        );
                    } else {
                        seenCommandIDs[builtID] = label;
                    }
                }
            }

            if (data.contains("telemetry") && data["telemetry"].is_array()) {
                for (const auto &src: data["telemetry"]) {
                    if (!src.contains("id") || !src.contains("name")) continue;
                    const uint16_t builtID = buildCommandID(shift, CATEGORY_TELEMETRY, src["id"].get<uint16_t>());
                    const std::string label = deviceName + "::" + src["name"].get<std::string>();
                    auto it = seenTelemetryIDs.find(builtID);
                    if (it != seenTelemetryIDs.end()) {
                        result.addError(
                            "Cross-device telemetry ID collision at shifted ID " + toHex(builtID) +
                            ": '" + label + "' conflicts with '" + it->second + "'"
                        );
                    } else {
                        seenTelemetryIDs[builtID] = label;
                    }
                }
            }

            if (data.contains("settings") && data["settings"].is_array()) {
                for (const auto &setting: data["settings"]) {
                    if (!setting.contains("id") || !setting.contains("name")) continue;
                    const uint16_t rawId = setting["id"].get<uint16_t>();
                    const std::string label = deviceName + "::" + setting["name"].get<std::string>();

                    const uint16_t getID = buildCommandID(shift, CATEGORY_SETTING_GET, rawId);
                    const uint16_t setID = buildCommandID(shift, CATEGORY_SETTING_SET, rawId);

                    auto itGet = seenCommandIDs.find(getID);
                    if (itGet != seenCommandIDs.end()) {
                        result.addError(
                            "Setting GET ID collision at " + toHex(getID) +
                            ": '" + label + "' conflicts with '" + itGet->second + "'"
                        );
                    } else {
                        seenCommandIDs[getID] = label + " (GET)";
                    }

                    auto itSet = seenCommandIDs.find(setID);
                    if (itSet != seenCommandIDs.end()) {
                        result.addError(
                            "Setting SET ID collision at " + toHex(setID) +
                            ": '" + label + "' conflicts with '" + itSet->second + "'"
                        );
                    } else {
                        seenCommandIDs[setID] = label + " (SET)";
                    }
                }
            }
        }
        return result;
    }
};
#endif //SMARTDRIVE_VALIDATOR_H
