from __future__ import annotations
from utils import (
    CPP_SIZEOF,
    TYPE_SIZES,
    get_optional_param_index,
    format_default_value,
    min_payload_size,
)

COMMAND_TYPE_SIZE = 2  # uint16_t


# ─────────────────────────────────────────────────────────────────────────────
# pack() helpers
# ─────────────────────────────────────────────────────────────────────────────


def _pack_param(i: int, param: dict, is_optional: bool) -> list[str]:
    """Generate the pack() lines for a single parameter."""
    t = param["type"]
    name = param["name"]
    lines: list[str] = []

    indent = "                "

    if is_optional:
        lines.append(f"{indent}// params[{i}]: {name} ({t}, optional)")
        lines.append(f"{indent}if (!cmd.params[{i}].isEmpty()) {{")
        inner = "                    "
    else:
        lines.append(f"{indent}// params[{i}]: {name} ({t}, required)")
        inner = indent

    if t == "STRING":
        lines.append(f"{inner}buffer[offset++] = cmd.params[{i}].getTypeAndSize();")
        lines.append(
            f"{inner}const size_t strDataSize{i} = cmd.params[{i}].getDataSize();"
        )
        lines.append(
            f"{inner}memcpy(&buffer[offset], cmd.params[{i}].getData(), strDataSize{i});"
        )
        lines.append(f"{inner}offset += strDataSize{i};")
    else:
        sz = CPP_SIZEOF[t]
        lines.append(
            f"{inner}memcpy(&buffer[offset], cmd.params[{i}].getData(), {sz});"
        )
        lines.append(f"{inner}offset += {sz};")

    if is_optional:
        lines.append(f"{indent}}}")

    return lines


def _pack_case(device_name: str, cmd: dict) -> list[str]:
    """Generate a single case block for pack()."""
    name = cmd["name"]
    params = cmd.get("params", [])
    optional_idx = get_optional_param_index(cmd)
    lines: list[str] = []

    lines.append(f"            case {device_name}CommandType::{name}: {{")

    if not params:
        lines.append(f"                // No parameters")
        lines.append(f"                return offset;")
    else:
        for i, param in enumerate(params):
            lines.extend(_pack_param(i, param, i == optional_idx))
        lines.append(f"                return offset;")

    lines.append(f"            }}\n")
    return lines


# ─────────────────────────────────────────────────────────────────────────────
# unpack() helpers
# ─────────────────────────────────────────────────────────────────────────────


def _unpack_string_param(i: int, is_optional: bool, default_val: str) -> list[str]:
    indent = "                "
    lines: list[str] = []

    if is_optional:
        lines.append(f"{indent}if (remainingBytes > 0) {{")
        inner = "                    "
        lines.append(f"{inner}const uint8_t typeAndSize{i} = buffer[offset++];")
        lines.append(f"{inner}cmdOut.params[{i}].setTypeAndSizeRaw(typeAndSize{i});")
        lines.append(f"{inner}remainingBytes--;")
        lines.append(
            f"{inner}const size_t strSize{i} = cmdOut.params[{i}].getDataSize();"
        )
        lines.append(f"{inner}if (remainingBytes < strSize{i}) return false;")
        lines.append(
            f"{inner}memcpy(cmdOut.params[{i}].getDataMutable(), &buffer[offset], strSize{i});"
        )
        lines.append(f"{inner}offset += strSize{i};")
        lines.append(f"{inner}remainingBytes -= strSize{i};")
        lines.append(f"{indent}}} else {{")
        lines.append(f'{inner}cmdOut.params[{i}] = "{default_val}";')
        lines.append(f"{indent}}}")
    else:
        lines.append(f"{indent}{{")
        inner = "                    "
        lines.append(f"{inner}if (remainingBytes < 1) return false;")
        lines.append(f"{inner}const uint8_t typeAndSize{i} = buffer[offset++];")
        lines.append(f"{inner}cmdOut.params[{i}].setTypeAndSizeRaw(typeAndSize{i});")
        lines.append(f"{inner}remainingBytes--;")
        lines.append(
            f"{inner}const size_t strSize{i} = cmdOut.params[{i}].getDataSize();"
        )
        lines.append(f"{inner}if (remainingBytes < strSize{i}) return false;")
        lines.append(
            f"{inner}memcpy(cmdOut.params[{i}].getDataMutable(), &buffer[offset], strSize{i});"
        )
        lines.append(f"{inner}offset += strSize{i};")
        lines.append(f"{inner}remainingBytes -= strSize{i};")
        lines.append(f"{indent}}}")

    return lines


def _unpack_fixed_param(
    i: int, t: str, is_optional: bool, default_val: str
) -> list[str]:
    sz = CPP_SIZEOF[t]
    indent = "                "
    lines: list[str] = []

    if is_optional:
        lines.append(f"{indent}if (remainingBytes >= {sz}) {{")
        inner = "                    "
        lines.append(f"{inner}cmdOut.params[{i}].setFrom_{t.lower()}(0);")
        lines.append(
            f"{inner}memcpy(cmdOut.params[{i}].getDataMutable(), &buffer[offset], {sz});"
        )
        lines.append(f"{inner}offset += {sz};")
        lines.append(f"{inner}remainingBytes -= {sz};")
        lines.append(f"{indent}}} else {{")
        formatted = format_default_value(t, default_val)
        lines.append(f"{inner}cmdOut.params[{i}] = {formatted};")
        lines.append(f"{indent}}}")
    else:
        lines.append(f"{indent}if (remainingBytes < {sz}) return false;")
        lines.append(
            f"{indent}memcpy(cmdOut.params[{i}].getDataMutable(), &buffer[offset], {sz});"
        )
        lines.append(f"{indent}offset += {sz};")
        lines.append(f"{indent}remainingBytes -= {sz};")

    return lines


def _unpack_case(device_name: str, cmd: dict) -> list[str]:
    """Generate a single case block for unpack()."""
    name = cmd["name"]
    params = cmd.get("params", [])
    optional_idx = get_optional_param_index(cmd)
    min_size = min_payload_size(cmd)
    lines: list[str] = []

    lines.append(f"            case {device_name}CommandType::{name}: {{")

    if not params:
        lines.append(f"                // No parameters")
        lines.append(f"                return true;")
    else:
        lines.append(f"                if (bufferSize < {min_size}) return false;")
        lines.append(f"                size_t remainingBytes = bufferSize - offset;\n")

        for i, param in enumerate(params):
            t = param["type"]
            default_val = str(param.get("default", ""))
            is_optional = i == optional_idx

            lines.append(
                f"                // params[{i}]: {param['name']} "
                f"({t}, {'optional' if is_optional else 'required'})"
            )

            if t == "STRING":
                lines.extend(_unpack_string_param(i, is_optional, default_val))
            else:
                lines.extend(_unpack_fixed_param(i, t, is_optional, default_val))

            lines.append("")

        lines.append(f"                return true;")

    lines.append(f"            }}\n")
    return lines


# ─────────────────────────────────────────────────────────────────────────────
# packedSize() helpers
# ─────────────────────────────────────────────────────────────────────────────


def _packed_size_for_cmd(cmd: dict) -> str:
    """Return the packed size expression for a command."""
    params = cmd.get("params", [])
    has_string = any(p["type"] == "STRING" for p in params)
    if has_string:
        return "0xFF"
    size = COMMAND_TYPE_SIZE + sum(TYPE_SIZES.get(p["type"], 0) for p in params)
    return str(size)


# ─────────────────────────────────────────────────────────────────────────────
# stringParamOffset() helpers
# ─────────────────────────────────────────────────────────────────────────────


def _string_param_offset(cmd: dict) -> int | None:
    """Return byte offset to the string param's typeAndSize byte, or None."""
    offset = 3  # seqNum(1) + commandType(2)
    for param in cmd.get("params", []):
        if param["type"] == "STRING":
            return offset
        offset += TYPE_SIZES.get(param["type"], 0)
    return None


# ─────────────────────────────────────────────────────────────────────────────
# Protocol-level cases (shared across pack, unpack, packedSize)
# ─────────────────────────────────────────────────────────────────────────────

PROTOCOL_PACK_CASES = """\
            case 0xFF00: {
                // Protocol-level GET_ALL_SETTINGS — no parameters
                return offset;
            }

            case 0xFD00: {
                // Protocol-level DISCOVER — no parameters
                return offset;
            }

            case 0xFD01: {
                // Protocol-level ANNOUNCE — one uint8_t (typeShift)
                memcpy(&buffer[offset], cmd.params[0].getData(), sizeof(uint8_t));
                offset += sizeof(uint8_t);
                return offset;
            }

            case 0xFC00: {
                // Protocol-level HEARTBEAT — no parameters
                return offset;
            }

            case 0xFC01: {
                // Protocol-level HEARTBEAT_ACK — one uint8_t (typeShift)
                memcpy(&buffer[offset], cmd.params[0].getData(), sizeof(uint8_t));
                offset += sizeof(uint8_t);
                return offset;
            }
"""

PROTOCOL_UNPACK_CASES = """\
            case 0xFF00: {
                // Protocol-level GET_ALL_SETTINGS — no parameters
                return true;
            }

            case 0xFD00: {
                // Protocol-level DISCOVER — no parameters
                return true;
            }

            case 0xFD01: {
                // Protocol-level ANNOUNCE — one uint8_t (typeShift)
                if (bufferSize - offset < sizeof(uint8_t)) return false;
                cmdOut.params[0] = uint8_t(0);
                memcpy(cmdOut.params[0].getDataMutable(), &buffer[offset], sizeof(uint8_t));
                offset += sizeof(uint8_t);
                return true;
            }

            case 0xFC00: {
                // Protocol-level HEARTBEAT — no parameters
                return true;
            }

            case 0xFC01: {
                // Protocol-level HEARTBEAT_ACK — one uint8_t (typeShift)
                if (bufferSize - offset < sizeof(uint8_t)) return false;
                cmdOut.params[0] = uint8_t(0);
                memcpy(cmdOut.params[0].getDataMutable(), &buffer[offset], sizeof(uint8_t));
                offset += sizeof(uint8_t);
                return true;
            }
"""

PROTOCOL_PACKED_SIZE_CASES = """\
            case 0xFF00: return 2;
            case 0xFD00: return 2;
            case 0xFD01: return 3;
            case 0xFC00: return 2;
            case 0xFC01: return 3;
"""


# ─────────────────────────────────────────────────────────────────────────────
# Main generator
# ─────────────────────────────────────────────────────────────────────────────


def generate(all_data: list[dict]) -> str:
    pack_cases: list[str] = [PROTOCOL_PACK_CASES]
    unpack_cases: list[str] = [PROTOCOL_UNPACK_CASES]
    packed_size_cases: list[str] = [PROTOCOL_PACKED_SIZE_CASES]
    string_offset_cases: list[str] = []

    for data in all_data:
        device_name = data["device"]
        type_shift = data["typeShift"]

        for cmd in data.get("commands", []):
            name = cmd["name"]

            # pack()
            pack_cases.extend(_pack_case(device_name, cmd))

            # unpack()
            unpack_cases.extend(_unpack_case(device_name, cmd))

            # packedSize()
            size_expr = _packed_size_for_cmd(cmd)
            packed_size_cases.append(
                f"            case {device_name}CommandType::{name}: return {size_expr};"
            )

            # stringParamOffset()
            offset = _string_param_offset(cmd)
            if offset is not None:
                string_offset_cases.append(
                    f"            case {device_name}CommandType::{name}: return {offset};"
                )

        # Telemetry GET cases — no parameters
        for src in data.get("telemetry", []):
            name = src["name"]
            pack_cases.append(
                f"            case {device_name}CommandType::GET_{name}: {{\n"
                f"                // Auto-generated telemetry request — no parameters\n"
                f"                return offset;\n"
                f"            }}\n"
            )
            unpack_cases.append(
                f"            case {device_name}CommandType::GET_{name}: {{\n"
                f"                // Auto-generated telemetry request — no parameters\n"
                f"                return true;\n"
                f"            }}\n"
            )
            packed_size_cases.append(
                f"            case {device_name}CommandType::GET_{name}: return 2;"
            )

        # Setting GET/SET cases
        for setting in data.get("settings", []):
            name = setting["name"]
            t = setting["type"]
            sz = CPP_SIZEOF.get(t, "")
            type_size_val = COMMAND_TYPE_SIZE + TYPE_SIZES.get(t, 0)

            # GET — no params
            for switch_list, ret in [
                (pack_cases, "return offset;"),
                (unpack_cases, "return true;"),
            ]:
                switch_list.append(
                    f"            case {device_name}CommandType::GET_SETTING_{name}: {{\n"
                    f"                // Auto-generated setting GET — no parameters\n"
                    f"                {ret}\n"
                    f"            }}\n"
                )
            packed_size_cases.append(
                f"            case {device_name}CommandType::GET_SETTING_{name}: return 2;"
            )

            # SET — one param
            if t == "STRING":
                set_pack = (
                    f"            case {device_name}CommandType::SET_SETTING_{name}: {{\n"
                    f"                buffer[offset++] = cmd.params[0].getTypeAndSize();\n"
                    f"                const size_t strDataSize = cmd.params[0].getDataSize();\n"
                    f"                memcpy(&buffer[offset], cmd.params[0].getData(), strDataSize);\n"
                    f"                offset += strDataSize;\n"
                    f"                return offset;\n"
                    f"            }}\n"
                )
                set_unpack = (
                    f"            case {device_name}CommandType::SET_SETTING_{name}: {{\n"
                    f"                if (bufferSize < 3) return false;\n"
                    f"                const uint8_t typeAndSize = buffer[offset++];\n"
                    f"                cmdOut.params[0].setTypeAndSizeRaw(typeAndSize);\n"
                    f"                const size_t strSize = cmdOut.params[0].getDataSize();\n"
                    f"                if (bufferSize - offset < strSize) return false;\n"
                    f"                memcpy(cmdOut.params[0].getDataMutable(), &buffer[offset], strSize);\n"
                    f"                offset += strSize;\n"
                    f"                return true;\n"
                    f"            }}\n"
                )
                packed_size_cases.append(
                    f"            case {device_name}CommandType::SET_SETTING_{name}: return 0xFF;"
                )
            else:
                set_pack = (
                    f"            case {device_name}CommandType::SET_SETTING_{name}: {{\n"
                    f"                memcpy(&buffer[offset], cmd.params[0].getData(), {sz});\n"
                    f"                offset += {sz};\n"
                    f"                return offset;\n"
                    f"            }}\n"
                )
                set_unpack = (
                    f"            case {device_name}CommandType::SET_SETTING_{name}: {{\n"
                    f"                if (bufferSize < 2 + {sz}) return false;\n"
                    f"                if (bufferSize - offset < {sz}) return false;\n"
                    f"                memcpy(cmdOut.params[0].getDataMutable(), &buffer[offset], {sz});\n"
                    f"                offset += {sz};\n"
                    f"                return true;\n"
                    f"            }}\n"
                )
                packed_size_cases.append(
                    f"            case {device_name}CommandType::SET_SETTING_{name}: return {type_size_val};"
                )

            pack_cases.append(set_pack)
            unpack_cases.append(set_unpack)

    pack_body = "\n".join(pack_cases)
    unpack_body = "\n".join(unpack_cases)
    packed_size_body = "\n".join(packed_size_cases)
    string_offset_body = "\n".join(string_offset_cases) if string_offset_cases else ""

    return f"""\
//
// CommandPacker.h
// AUTO-GENERATED BY OmniPlexus CommandGenerator - DO NOT EDIT
//

#pragma once

#include "CommandTypes.h"
#include "opx/shared/core/platform.h"
#include "opx/shared/types/ProtocolTypes.h"

class CommandPacker {{
public:

    // Serialize a Command into buffer.
    // Returns number of bytes written, 0 on failure.
    static size_t pack(const Command& cmd, uint8_t* buffer) {{
        size_t offset = 0;

        // Write commandType (little-endian)
        buffer[offset++] = cmd.commandType & 0xFF;
        buffer[offset++] = (cmd.commandType >> 8) & 0xFF;

        switch(cmd.commandType) {{

{pack_body}
            default:
                return 0; // Unknown command type
        }}
    }}

    // Deserialize a Command from buffer.
    // Returns true on success.
    static bool unpack(const uint8_t* buffer, size_t bufferSize, Command& cmdOut) {{
        if (bufferSize < 2) return false;

        size_t offset = 0;

        // Read commandType (little-endian)
        const uint16_t cmdType =
            static_cast<uint16_t>(buffer[offset]) |
            (static_cast<uint16_t>(buffer[offset + 1]) << 8);
        offset += 2;
        cmdOut.commandType = cmdType;

        switch(cmdType) {{

{unpack_body}
            default:
                return false; // Unknown command type
        }}
    }}

    // NOTE TO CODE GENERATOR MAINTAINER:
    // packedSize() and stringParamOffset() are kept in sync with pack() by the generator.
    // When adding a new command to the manifest, regenerate to update all three.

    // Returns total bytes that pack() writes for the given commandType,
    // INCLUDING the 2-byte commandType prefix.
    // Returns 0xFF (STRING_SENTINEL) if the command has a string parameter.
    // Returns 0 for unknown command types.
    static uint8_t packedSize(uint16_t commandType) {{
        switch (commandType) {{
{packed_size_body}
            default: return 0;
        }}
    }}

    // Returns the byte offset from the start of the payload (after the header byte)
    // where the string parameter's typeAndSize byte can be found.
    // Only meaningful when packedSize() returns 0xFF.
    // Returns 0 for non-string commands.
    static uint8_t stringParamOffset(uint16_t commandType) {{
        switch (commandType) {{
{string_offset_body}
            default: return 0;
        }}
    }}

}}; // class CommandPacker
"""
