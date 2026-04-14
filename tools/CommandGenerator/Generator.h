#ifndef COMMANDGENERATOR_GENERATOR_H
#define COMMANDGENERATOR_GENERATOR_H

#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "lib/nlohmann/json.hpp"

using json = nlohmann::json;

static const int COMMAND_TYPE_SIZE = 2; // uint16_t

class Generator {
private:
    // Write a string to a file, throws on failure
    static void writeFile(const std::string &path, const std::string &content) {
        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file for writing: " + path);
        }
        file << content;
        file.close();
    }

    // Format uint16_t as "0x0001"
    static std::string toHex(const uint16_t value) {
        std::ostringstream oss;
        oss << "0x" << std::uppercase << std::hex
                << std::setw(4) << std::setfill('0') << value;
        return oss.str();
    }

    //convery from UPPER_SNAKE_CASE to camelCase
    static std::string toCamelCase(const std::string &upperSnake) {
        std::string result;
        bool capitalizeNext = false;

        for (size_t i = 0; i < upperSnake.size(); ++i) {
            const char c = upperSnake[i];
            if (c == '_') {
                capitalizeNext = true;
            } else {
                if (i == 0) {
                    // First character always lowercase
                    result += static_cast<char>(std::tolower(c));
                } else if (capitalizeNext) {
                    result += static_cast<char>(std::toupper(c));
                    capitalizeNext = false;
                } else {
                    result += static_cast<char>(std::tolower(c));
                }
            }
        }
        return result;
    }

    static std::string cppTypeForJsonType(const std::string &type) {
        if (type == "UINT8")  return "uint8_t";
        if (type == "INT8")   return "int8_t";
        if (type == "UINT16") return "uint16_t";
        if (type == "INT16")  return "int16_t";
        if (type == "UINT32") return "uint32_t";
        if (type == "INT32")  return "int32_t";
        if (type == "FLOAT")  return "float";
        if (type == "STRING") return "const char*";
        return "uint8_t";
    }

    // Map a JSON type string to the C++ default initializer for unpack
    static std::string defaultForType(const std::string &type) {
        if (type == "FLOAT") return "0.0f";
        if (type == "INT8") return "int8_t(0)";
        if (type == "UINT8") return "uint8_t(0)";
        if (type == "INT16") return "int16_t(0)";
        if (type == "UINT16") return "uint16_t(0)";
        if (type == "INT32") return "int32_t(0)";
        if (type == "UINT32") return "uint32_t(0)";
        if (type == "STRING") return "\"\"";
        return "0";
    }

    // Map a JSON type string to sizeof expression
    // string uses getDataSize()
    static std::string sizeofForType(const std::string &type) {
        if (type == "FLOAT") return "sizeof(float)";
        if (type == "INT8") return "sizeof(int8_t)";
        if (type == "UINT8") return "sizeof(uint8_t)";
        if (type == "INT16") return "sizeof(int16_t)";
        if (type == "UINT16") return "sizeof(uint16_t)";
        if (type == "INT32") return "sizeof(int32_t)";
        if (type == "UINT32") return "sizeof(uint32_t)";
        return "0";
    }

    // Map a JSON type string to the ValueType enum name
    static std::string valueTypeEnum(const std::string &type) {
        return "ValueType::" + type;
    }

    // Compute the minimum expected payload size for a command (required params only)
    static size_t minPayloadSize(const json &cmd) {
        size_t size = COMMAND_TYPE_SIZE;
        if (!cmd.contains("params")) return size;

        for (const auto &param: cmd["params"]) {
            if (!param["required"].get<bool>()) continue;

            const std::string type = param["type"].get<std::string>();
            if (type == "FLOAT") size += sizeof(float);
            else if (type == "INT8") size += sizeof(int8_t);
            else if (type == "UINT8") size += sizeof(uint8_t);
            else if (type == "INT16") size += sizeof(int16_t);
            else if (type == "UINT16") size += sizeof(uint16_t);
            else if (type == "INT32") size += sizeof(int32_t);
            else if (type == "UINT32") size += sizeof(uint32_t);
            else if (type == "STRING") size += 2; // At minimum 2 bytes(1 for typeAndSize and null terminator for empty string)
        }
        return size;
    }

    static size_t readExistingMaxParamSize(const std::string& headerDir) {
        const std::string path = headerDir + "GeneratedConfig.h";
        std::ifstream file(path);
        if (!file.is_open()) return 0;

        std::string line;
        while (std::getline(file, line)) {
            const std::string needle = "constexpr uint8_t MAX_PACKED_PARAM_SIZE = ";
            const size_t pos = line.find(needle);
            if (pos != std::string::npos) {
                return static_cast<size_t>(
                    std::stoul(line.substr(pos + needle.size()))
                );
            }
        }
        return 0;
    }

    static size_t maxPackedParamSize(const json& data) {
        size_t maxSize = 0;

        for (const auto& cmd: data["commands"]) {
            if (!cmd.contains("params")) continue;
            size_t cmdParamSize = 0;

            for (const auto& param: cmd["params"]) {
                const std::string type = param["type"].get<std::string>();
                if (type == "FLOAT") cmdParamSize += sizeof(float);
                else if (type == "INT8") cmdParamSize += sizeof(int8_t);
                else if (type == "UINT8") cmdParamSize += sizeof(uint8_t);
                else if (type == "INT16") cmdParamSize += sizeof(int16_t);
                else if (type == "UINT16") cmdParamSize += sizeof(uint16_t);
                else if (type == "INT32") cmdParamSize += sizeof(int32_t);
                else if (type == "UINT32") cmdParamSize += sizeof(uint32_t);
                else if (type == "STRING") {
                    const size_t maxLen = param.contains("maxLen") ? param["maxLen"].get<size_t>() : 15;
                    cmdParamSize += 1 + maxLen; //1 is for the typeAndSize byte
                }
            }

            maxSize = std::max(maxSize, cmdParamSize);
        }

        return maxSize;
    }

    static bool hasOptionalParam(const json &cmd) {
        if (!cmd.contains("params")) return false;

        for (const auto &param: cmd["params"]) {
            if (param.contains("required") && !param["required"].get<bool>()) {
                return true;
            }
        }
        return false;
    }

    static int getOptionalParamIndex(const json &cmd) {
        if (!cmd.contains("params")) return -1;

        for (size_t i = 0; i < cmd["params"].size(); ++i) {
            if (cmd["params"][i].contains("required") &&
                !cmd["params"][i]["required"].get<bool>()) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }


    // ─────────────────────────────────────────────────────────────────────
    // File Generators
    // ─────────────────────────────────────────────────────────────────────

    static std::string generateCommandTypesContent(const json &data) {
        const std::string deviceName = data["device"].get<std::string>();
        const std::string sourceName = deviceName + ".json";

        std::ostringstream out;

        out << "//\n";
        out << "// CommandTypes.h\n";
        out << "// AUTO-GENERATED BY SmartDrive CommandGenerator - DO NOT EDIT\n";
        out << "// Source: " << sourceName << "\n";
        out << "//\n\n";
        out << "#ifndef SMARTDRIVE_COMMANDTYPES_H\n";
        out << "#define SMARTDRIVE_COMMANDTYPES_H\n\n";
        out << "#include \"../core/platform.h\"\n\n";
        out << "namespace CommandType {\n\n";

        for (const auto &cmd: data["commands"]) {
            const std::string name = cmd["name"].get<std::string>();
            const uint16_t id = cmd["id"].get<uint16_t>();

            // Include description as comment if available
            if (cmd.contains("description") && !cmd["description"].get<std::string>().empty()) {
                out << "    // " << cmd["description"].get<std::string>() << "\n";
            }
            out << "    constexpr uint16_t " << name
                    << " = " << toHex(id) << ";\n\n";
        }

        out << "} // namespace CommandType\n\n";
        out << "#endif // SMARTDRIVE_COMMANDTYPES_H\n";

        return out.str();
    }

    static std::string generateTelemetrySourceIDsContent(const json& data) {
        const std::string deviceName = data["device"].get<std::string>();
        const std::string sourceName = deviceName + ".json";

        std::ostringstream out;

        out << "//\n";
        out << "// TelemetrySourceIDs.h\n";
        out << "// AUTO-GENERATED BY SmartDrive CommandGenerator - DO NOT EDIT\n";
        out << "// Source: " << sourceName << "\n";
        out << "//\n\n";
        out << "#ifndef SMARTDRIVE_TELEMETRYSOURCEIDS_H\n";
        out << "#define SMARTDRIVE_TELEMETRYSOURCEIDS_H\n\n";
        out << "#include \"../core/platform.h\"\n\n";
        out << "namespace TelemetrySource {\n\n";

        for (const auto& source : data["telemetry"]) {
            const std::string name = source["name"].get<std::string>();
            const uint16_t id = source["id"].get<uint16_t>();

            if (source.contains("description") && !source["description"].get<std::string>().empty()) {
                out << "    // " << source["description"].get<std::string>() << "\n";
            }
            out << "    constexpr uint16_t " << name << " = " << toHex(id) << ";\n\n";
        }

        out << "} // namespace TelemetrySource\n\n";
        out << "#endif // SMARTDRIVE_TELEMETRYSOURCEIDS_H\n";

        return out.str();
    }

    static std::string generateCommandPackerContent(const json &data) {
        const std::string deviceName = data["device"].get<std::string>();
        const std::string sourceName = deviceName + ".json";

        std::ostringstream out;

        out << "//\n";
        out << "// CommandPacker.h\n";
        out << "// AUTO-GENERATED BY SmartDrive CommandGenerator - DO NOT EDIT\n";
        out << "// Source: " << sourceName << "\n";
        out << "//\n\n";
        out << "#ifndef SMARTDRIVE_COMMANDPACKER_H\n";
        out << "#define SMARTDRIVE_COMMANDPACKER_H\n\n";
        out << "#include \"../core/platform.h\"\n";
        out << "#include \"CommandTypes.h\"\n";
        out << "#include \"../types/ProtocolTypes.h\"\n\n";
        out << "class CommandPacker {\n";
        out << "public:\n\n";

        // ── pack() ──────────────────────────────────────────────────────
        out << "    // Serialize a Command into buffer\n";
        out << "    // Returns number of bytes written, 0 on failure\n";
        out << "    static size_t pack(const Command& cmd, uint8_t* buffer) {\n";
        out << "        size_t offset = 0;\n\n";
        out << "        // Write commandType (little-endian)\n";
        out << "        buffer[offset++] = cmd.commandType & 0xFF;\n";
        out << "        buffer[offset++] = (cmd.commandType >> 8) & 0xFF;\n\n";
        out << "        switch(cmd.commandType) {\n\n";

        for (const auto &cmd: data["commands"]) {
            const std::string name = cmd["name"].get<std::string>();
            const int optionalIdx = getOptionalParamIndex(cmd);

            out << "            case CommandType::" << name << ": {\n";

            if (!cmd.contains("params") || cmd["params"].empty()) {
                out << "                // No parameters\n";
                out << "                return offset;\n";
            } else {
                for (size_t i = 0; i < cmd["params"].size(); ++i) {
                    const auto &param = cmd["params"][i];
                    const std::string paramName = param["name"].get<std::string>();
                    const std::string type = param["type"].get<std::string>();
                    const bool isOptional = (static_cast<int>(i) == optionalIdx);

                    if (isOptional) {
                        out << "                // params[" << i << "]: " << paramName
                                << " (" << type << ", optional)\n";
                        out << "                if (!cmd.params[" << i << "].isEmpty()) {\n";
                        out << "                    ";
                    } else {
                        out << "                // params[" << i << "]: " << paramName
                                << " (" << type << ", required)\n";
                        out << "                ";
                    }

                    if (type == "STRING") {
                        // Write typeAndSize byte first
                        out << "buffer[offset++] = cmd.params[" << i << "].getTypeAndSize();\n";
                        if (isOptional) out << "                    ";

                        // Then write string data
                        out << "const size_t strDataSize" << i << " = cmd.params["
                            << i << "].getDataSize();\n";
                        if (isOptional) out << "                    ";
                        out << "memcpy(&buffer[offset], cmd.params["
                            << i << "].getData(), strDataSize" << i << ");\n";
                        if (isOptional) out << "                    ";
                        out << "offset += strDataSize" << i << ";\n";
                    } else {
                        out << "memcpy(&buffer[offset], cmd.params["
                                << i << "].getData(), " << sizeofForType(type) << ");\n";
                        if (isOptional) out << "                    ";
                        out << "offset += " << sizeofForType(type) << ";\n";
                    }

                    if (isOptional) {
                        out << "                }\n";
                    }
                }
                out << "                return offset;\n";
            }

            out << "            }\n\n";
        }

        out << "            default:\n";
        out << "                return 0; // Unknown command type\n";
        out << "        }\n";
        out << "    }\n\n";

        // ── unpack() ─────────────────────────────────────────────────────
        out << "    // Deserialize a Command from buffer\n";
        out << "    // Returns true on success\n";
        out << "    static bool unpack(const uint8_t* buffer, size_t bufferSize, Command& cmdOut) {\n";
        out << "        if (bufferSize < 2) return false;\n\n";
        out << "        size_t offset = 0;\n\n";
        out << "        // Read commandType (little-endian)\n";
        out << "        const uint16_t cmdType =\n";
        out << "            static_cast<uint16_t>(buffer[offset]) |\n";
        out << "            (static_cast<uint16_t>(buffer[offset + 1]) << 8);\n";
        out << "        offset += 2;\n";
        out << "        cmdOut.commandType = cmdType;\n\n";
        out << "        switch(cmdType) {\n\n";

        for (const auto &cmd: data["commands"]) {
            const std::string name = cmd["name"].get<std::string>();
            const size_t minSize = minPayloadSize(cmd);
            const int optionalIdx = getOptionalParamIndex(cmd);

            out << "            case CommandType::" << name << ": {\n";

            if (!cmd.contains("params") || cmd["params"].empty()) {
                out << "                // No parameters\n";
                out << "                return true;\n";
            } else {
                out << "                if (bufferSize < " << minSize << ") return false;\n";
                out << "                size_t remainingBytes = bufferSize - offset;\n\n";

                for (size_t i = 0; i < cmd["params"].size(); ++i) {
                    const auto &param = cmd["params"][i];
                    const std::string paramName = param["name"].get<std::string>();
                    const std::string type = param["type"].get<std::string>();
                    const bool isOptional = (static_cast<int>(i) == optionalIdx);
                    const std::string defVal = defaultForType(type);
                    const std::string defaultValue = param.contains("default")
                                                         ? param["default"].get<std::string>()
                                                         : "";

                    out << "                // params[" << i << "]: " << paramName
                            << " (" << type << ", " << (isOptional ? "optional" : "required") << ")\n";

                    if (isOptional) {
                        // Optional parameter - check remaining bytes
                        if (type == "STRING") {
                            out << "                if (remainingBytes > 0) {\n";
                            out << "                    // Read typeAndSize byte first\n";
                            out << "                    const uint8_t typeAndSize" << i << " = buffer[offset++];\n";
                            out << "                    cmdOut.params[" << i << "].setTypeAndSizeRaw(typeAndSize" << i << ");\n";
                            out << "                    remainingBytes--;\n";
                            out << "                    \n";
                            out << "                    // Now read string data\n";
                            out << "                    const size_t strSize" << i
                                << " = cmdOut.params[" << i << "].getDataSize();\n";
                            out << "                    if (remainingBytes < strSize" << i << ") return false;\n";
                            out << "                    memcpy(cmdOut.params[" << i
                                << "].getDataMutable(), &buffer[offset], strSize" << i << ");\n";
                            out << "                    offset += strSize" << i << ";\n";
                            out << "                    remainingBytes -= strSize" << i << ";\n";
                            out << "                } else {\n";
                            out << "                    // Use default\n";
                            out << "                    cmdOut.params[" << i << "] = \""
                                << defaultValue << "\";\n";
                            out << "                }\n";
                        } else {
                            out << "                if (remainingBytes >= " << sizeofForType(type) << ") {\n";
                            out << "                    cmdOut.params[" << i << "] = " << defVal << ";\n";
                            out << "                    memcpy(cmdOut.params[" << i
                                    << "].getDataMutable(), &buffer[offset], "
                                    << sizeofForType(type) << ");\n";
                            out << "                    offset += " << sizeofForType(type) << ";\n";
                            out << "                    remainingBytes -= " << sizeofForType(type) << ";\n";
                            out << "                } else {\n";
                            out << "                    // Use default\n";

                            // Format default value based on type
                            std::string formattedDefault;
                            if (type == "FLOAT") {
                                formattedDefault = defaultValue + "f";
                            } else if (type == "INT8") {
                                formattedDefault = "int8_t(" + defaultValue + ")";
                            } else if (type == "UINT8") {
                                formattedDefault = "uint8_t(" + defaultValue + ")";
                            } else if (type == "INT16") {
                                formattedDefault = "int16_t(" + defaultValue + ")";
                            } else if (type == "UINT16") {
                                formattedDefault = "uint16_t(" + defaultValue + ")";
                            } else if (type == "INT32") {
                                formattedDefault = "int32_t(" + defaultValue + ")";
                            } else if (type == "UINT32") {
                                formattedDefault = "uint32_t(" + defaultValue + ")";
                            } else {
                                formattedDefault = defaultValue;
                            }

                            out << "                    cmdOut.params[" << i << "] = "
                                    << formattedDefault << ";\n";
                            out << "                }\n";
                        }
                    } else {
                        // Required parameter
                        out << "                cmdOut.params[" << i << "] = " << defVal << ";\n";

                        if (type == "STRING") {
                            out << "                {\n";
                            out << "                    // Read typeAndSize byte first\n";
                            out << "                    if (remainingBytes < 1) return false;\n";
                            out << "                    const uint8_t typeAndSize" << i << " = buffer[offset++];\n";
                            out << "                    cmdOut.params[" << i << "].setTypeAndSizeRaw(typeAndSize" << i << ");\n";
                            out << "                    remainingBytes--;\n";
                            out << "                    \n";
                            out << "                    // Now read string data\n";
                            out << "                    const size_t strSize" << i
                                << " = cmdOut.params[" << i << "].getDataSize();\n";
                            out << "                    if (remainingBytes < strSize" << i << ") return false;\n";
                            out << "                    memcpy(cmdOut.params[" << i
                                << "].getDataMutable(), &buffer[offset], strSize" << i << ");\n";
                            out << "                    offset += strSize" << i << ";\n";
                            out << "                    remainingBytes -= strSize" << i << ";\n";
                            out << "                }\n";
                        } else {
                            out << "                if (remainingBytes < " << sizeofForType(type) <<
                                    ") return false;\n";
                            out << "                memcpy(cmdOut.params[" << i
                                    << "].getDataMutable(), &buffer[offset], "
                                    << sizeofForType(type) << ");\n";
                            out << "                offset += " << sizeofForType(type) << ";\n";
                            out << "                remainingBytes -= " << sizeofForType(type) << ";\n";
                        }
                    }

                    out << "\n";
                }

                out << "                return true;\n";
            }

            out << "            }\n\n";
        }

        out << "            default:\n";
        out << "                return false; // Unknown command type\n";
        out << "        }\n";
        out << "    }\n\n";

        out << "}; // class CommandPacker\n\n";
        out << "#endif // SMARTDRIVE_COMMANDPACKER_H\n";

        return out.str();
    }

    static std::string generateCommandRegistryContent(const json &data) {
        const std::string deviceName = data["device"].get<std::string>();
        const std::string sourceName = deviceName + ".json";

        std::ostringstream out;

        out << "//\n";
        out << "// CommandRegistry.cpp\n";
        out << "// AUTO-GENERATED BY SmartDrive CommandGenerator - DO NOT EDIT\n";
        out << "// Source: " << sourceName << "\n";
        out << "//\n\n";
        out << "#include \"../../include/smartdrive/registry/CommandRegistry.h\"\n";
        out << "#include \"../../include/smartdrive/generated/CommandTypes.h\"\n\n";
        out << "#ifndef EMBEDDED_BUILD\n\n";
        out << "void CommandRegistry::initialize() {\n\n";

        for (const auto &cmd: data["commands"]) {
            const std::string name = cmd["name"].get<std::string>();
            const std::string desc = cmd.contains("description")
                                         ? cmd["description"].get<std::string>()
                                         : "";

            out << "    // " << name << "\n";
            out << "    registerCommand({\n";
            out << "        CommandType::" << name << ",\n";
            out << "        \"" << name << "\",\n";
            out << "        \"" << desc << "\",\n";
            out << "        {\n";

            if (cmd.contains("params")) {
                for (const auto &param: cmd["params"]) {
                    const std::string paramName = param["name"].get<std::string>();
                    const std::string type = param["type"].get<std::string>();
                    const bool required = param["required"].get<bool>();
                    const std::string paramDesc = param.contains("description")
                                                      ? param["description"].get<std::string>()
                                                      : "";
                    const size_t maxLength = (type == "STRING" && param.contains("max_length"))
                                                 ? param["max_length"].get<size_t>()
                                                 : 0;
                    const std::string defVal = param.contains("default")
                                                   ? param["default"].get<std::string>()
                                                   : "";

                    out << "            {\"" << paramName << "\", "
                            << valueTypeEnum(type) << ", "
                            << (required ? "true" : "false") << ", "
                            << "\"" << paramDesc << "\", "
                            << maxLength << ", "
                            << "\"" << defVal << "\"},\n";
                }
            }

            out << "        }\n";
            out << "    });\n\n";
        }

        out << "} // CommandRegistry::initialize()\n\n";
        out << "#endif // EMBEDDED_BUILD\n";

        return out.str();
    }

    static std::string generateControllerContent(const json &data) {
        const std::string deviceName  = data["device"].get<std::string>();
        const std::string sourceName  = deviceName + ".json";
        const std::string className   = deviceName + "Controller";
        const std::string guardName   = "SMARTDRIVE_" + [&]() {
            std::string upper = className;
            for (char &c : upper) c = static_cast<char>(std::toupper(c));
            return upper;
        }() + "_H";

        std::ostringstream out;

        out << "//\n";
        out << "// " << className << ".h\n";
        out << "// AUTO-GENERATED BY SmartDrive CommandGenerator - DO NOT EDIT\n";
        out << "// Source: " << sourceName << "\n";
        out << "//\n\n";
        out << "#ifndef " << guardName << "\n";
        out << "#define " << guardName << "\n\n";
        out << "#include \"../core/platform.h\"\n";
        out << "#include \"../core/CommunicationManager.h\"\n";
        out << "#include \"../types/ProtocolTypes.h\"\n";
        out << "#include \"CommandTypes.h\"\n\n";
        out << "class " << className << " {\n";
        out << "private:\n";
        out << "    CommunicationManager& comms;\n\n";
        out << "public:\n";
        out << "    explicit " << className << "(CommunicationManager& comms) : comms(comms) {}\n\n";

        for (const auto &cmd: data["commands"]) {
            const std::string cmdName    = cmd["name"].get<std::string>();
            const std::string methodName = toCamelCase(cmdName);

            // Build parameter list for the method signature
            std::ostringstream paramList;
            bool firstParam = true;

            // Collect required params first, then optional (with default)
            if (cmd.contains("params")) {
                for (const auto &param: cmd["params"]) {
                    const std::string paramName  = param["name"].get<std::string>();
                    const std::string type       = param["type"].get<std::string>();
                    const bool isRequired        = param["required"].get<bool>();
                    const std::string cppType    = cppTypeForJsonType(type);

                    if (!firstParam) paramList << ", ";
                    firstParam = false;

                    paramList << cppType << " " << paramName;

                    // Optional params get a C++ default value in the signature
                    if (!isRequired && param.contains("default")) {
                        const std::string defVal = param["default"].get<std::string>();
                        if (type == "STRING") {
                            paramList << " = \"" << defVal << "\"";
                        } else if (type == "FLOAT") {
                            paramList << " = " << defVal << "f";
                        } else if (type == "UINT8") {
                            paramList << " = uint8_t(" << defVal << ")";
                        } else if (type == "INT8") {
                            paramList << " = int8_t(" << defVal << ")";
                        } else if (type == "UINT16") {
                            paramList << " = uint16_t(" << defVal << ")";
                        } else if (type == "INT16") {
                            paramList << " = int16_t(" << defVal << ")";
                        } else if (type == "UINT32") {
                            paramList << " = uint32_t(" << defVal << ")";
                        } else if (type == "INT32") {
                            paramList << " = int32_t(" << defVal << ")";
                        } else {
                            paramList << " = " << defVal;
                        }
                    }
                }
            }

            // Write description as doc comment
            if (cmd.contains("description") && !cmd["description"].get<std::string>().empty()) {
                out << "    // " << cmd["description"].get<std::string>() << "\n";
            }

            // Write method signature
            out << "    bool " << methodName << "(" << paramList.str() << ") {\n";

            // Build Command struct inside the method
            out << "        Command cmd;\n";
            out << "        cmd.commandType = CommandType::" << cmdName << ";\n";

            if (cmd.contains("params") && !cmd["params"].empty()) {
                for (size_t i = 0; i < cmd["params"].size(); ++i) {
                    const auto &param        = cmd["params"][i];
                    const std::string pName  = param["name"].get<std::string>();
                    const std::string type   = param["type"].get<std::string>();
                    const bool isRequired    = param["required"].get<bool>();

                    // Only assign if required, or if optional and not empty
                    if (isRequired) {
                        out << "        cmd.params[" << i << "] = " << pName << ";\n";
                    } else {
                        // Optional — only set if the user passed a non-default value
                        // We assign it anyway; CommandPacker will skip it if isEmpty()
                        // But since ValueSource assignment always sets a type, we assign directly
                        out << "        cmd.params[" << i << "] = " << pName << ";\n";
                    }
                }
            }

            const bool requiresAck = cmd["acknowledges"].get<bool>();
            const std::string ackStr = requiresAck ? "true" : "false";

            out << "        return comms.dispatch(cmd, " << ackStr << ");\n";
            out << "    }\n\n";
        }

        out << "}; // class " << className << "\n\n";
        out << "#endif // " << guardName << "\n";

        return out.str();
    }

    static std::string generateProtocolConfigContent(const json& data, size_t existingMax) {
        const std::string deviceName = data["device"].get<std::string>();
        const size_t maxParamSize = std::max(maxPackedParamSize(data), existingMax);

        std::ostringstream out;

        out << "//\n";
        out << "// GeneratedConfig.h\n";
        out << "// AUTO-GENERATED BY SmartDrive CommandGenerator - DO NOT EDIT\n";
        out << "// Source: " << deviceName << ".json\n";
        out << "//\n\n";
        out << "#include \"../core/platform.h\"\n";
        out << "#ifndef SMARTDRIVE_GENERATEDCONFIG_H\n";
        out << "#define SMARTDRIVE_GENERATEDCONFIG_H\n\n";
        out << "// Maximum packed parameter bytes across all commands in this device.\n";
        out << "// Used to size PackedCommand::paramBytes in CommandQueue.\n";
        out << "constexpr uint8_t MAX_PACKED_PARAM_SIZE = " << maxParamSize << ";\n\n";
        out << "#endif // SMARTDRIVE_GENERATEDCONFIG_H\n";

        return out.str();
    }

public:
    // Generate all three output files
    // headerOutputDir → where CommandTypes.h and CommandPacker.h are written
    // sourceOutputDir → where CommandRegistry.cpp is written
    static void generate(const json &data,
                         const std::string &headerOutputDir,
                         const std::string &sourceOutputDir) {
        // Normalize paths (ensure trailing slash)
        std::string headerDir = headerOutputDir;
        std::string sourceDir = sourceOutputDir;
        if (!headerDir.empty() && headerDir.back() != '/') headerDir += '/';
        if (!sourceDir.empty() && sourceDir.back() != '/') sourceDir += '/';

        const std::string deviceName = data["device"].get<std::string>();

        // Generate CommandTypes.h
        const std::string commandTypesPath = headerDir + "CommandTypes.h";
        writeFile(commandTypesPath, generateCommandTypesContent(data));
        std::cout << "  ✓ Generated: " << commandTypesPath << "\n";

        // Generate CommandPacker.h
        const std::string commandPackerPath = headerDir + "CommandPacker.h";
        writeFile(commandPackerPath, generateCommandPackerContent(data));
        std::cout << "  ✓ Generated: " << commandPackerPath << "\n";

        // Generate CommandRegistry.cpp
        const std::string commandRegistryPath = sourceDir + "CommandRegistry.cpp";
        writeFile(commandRegistryPath, generateCommandRegistryContent(data));
        std::cout << "  ✓ Generated: " << commandRegistryPath << "\n";

        //Generate <DeviceName>Controller.h
        const std::string controllerPath = headerDir + deviceName + "Controller.h";
        writeFile(controllerPath, generateControllerContent(data));
        std::cout << "  ✓ Generated: " << controllerPath << "\n";

        //Generate <DeviceName>GeneratedConfig.h
        const size_t existingMax = readExistingMaxParamSize(headerDir);
        const std::string generatedConfigPath = headerDir + "GeneratedConfig.h";
        writeFile(generatedConfigPath, generateProtocolConfigContent(data, existingMax));
        std::cout << "  ✓ Generated: " << generatedConfigPath << "\n";

        // Generate TelemetrySourceIDs.h (only if telemetry sources are defined)
        if (data.contains("telemetry") && !data["telemetry"].empty()) {
            const std::string telemetrySourceIDsPath = headerDir + "TelemetrySourceIDs.h";
            writeFile(telemetrySourceIDsPath, generateTelemetrySourceIDsContent(data));
            std::cout << "  ✓ Generated: " << telemetrySourceIDsPath << "\n";
        }
    }
};

#endif // COMMANDGENERATOR_GENERATOR_H
