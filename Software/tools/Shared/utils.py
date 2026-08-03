from __future__ import annotations

VALID_TYPES: set[str] = {
    "UINT8",
    "INT8",
    "FLOAT",
    "UINT16",
    "INT16",
    "UINT32",
    "INT32",
    "STRING",
}

CATEGORY_COMMAND = 0x0
CATEGORY_TELEMETRY = 0x1
CATEGORY_SETTING_GET = 0x2
CATEGORY_SETTING_SET = 0x3

TYPE_SIZES: dict[str, int] = {
    "FLOAT": 4,
    "INT8": 1,
    "UINT8": 1,
    "INT16": 2,
    "UINT16": 2,
    "INT32": 4,
    "UINT32": 4,
}

CPP_TYPES: dict[str, str] = {
    "UINT8": "uint8_t",
    "INT8": "int8_t",
    "UINT16": "uint16_t",
    "INT16": "int16_t",
    "UINT32": "uint32_t",
    "INT32": "int32_t",
    "FLOAT": "float",
    "STRING": "const char*",
}

CPP_DEFAULTS: dict[str, str] = {
    "FLOAT": "0.0f",
    "INT8": "int8_t(0)",
    "UINT8": "uint8_t(0)",
    "INT16": "int16_t(0)",
    "UINT16": "uint16_t(0)",
    "INT32": "int32_t(0)",
    "UINT32": "uint32_t(0)",
    "STRING": '""',
}

CPP_SIZEOF: dict[str, str] = {
    "FLOAT": "sizeof(float)",
    "INT8": "sizeof(int8_t)",
    "UINT8": "sizeof(uint8_t)",
    "INT16": "sizeof(int16_t)",
    "UINT16": "sizeof(uint16_t)",
    "INT32": "sizeof(int32_t)",
    "UINT32": "sizeof(uint32_t)",
}

VALUE_TYPE_ENUM: dict[str, str] = {t: f"ValueType::{t}" for t in VALID_TYPES}

# ─────────────────────────────────────────────────────────────────────────────
# ID helpers
# ─────────────────────────────────────────────────────────────────────────────


def build_command_id(type_shift: int, category: int, local_id: int) -> int:
    """Build a full 16-bit command ID from typeShift, category, and local ID."""
    return ((type_shift << 11) | (category << 8) | local_id) & 0xFFFF


def shifted_id(type_shift: int, raw_id: int) -> int:
    """Apply typeShift to a raw telemetry or setting ID."""
    return (raw_id | (type_shift << 8)) & 0xFFFF


# ─────────────────────────────────────────────────────────────────────────────
# Formatting helpers
# ─────────────────────────────────────────────────────────────────────────────


def to_hex(value: int) -> str:
    """Format an integer as a 4-digit uppercase hex string e.g. 0x001A."""
    return f"0x{value:04X}"


def to_camel_case(upper_snake: str) -> str:
    """Convert UPPER_SNAKE_CASE to camelCase."""
    parts = upper_snake.split("_")
    if not parts:
        return upper_snake
    return parts[0].lower() + "".join(p.capitalize() for p in parts[1:])


def to_pascal_case(upper_snake: str) -> str:
    """Convert UPPER_SNAKE_CASE to PascalCase."""
    return "".join(p.capitalize() for p in upper_snake.split("_"))


def is_valid_identifier(name: str) -> bool:
    """Check that a name is a valid C identifier (letter/underscore start,
    alphanumeric/underscore body)."""
    if not name:
        return False
    if not (name[0].isalpha() or name[0] == "_"):
        return False
    return all(c.isalnum() or c == "_" for c in name)


def is_upper_snake_case(name: str) -> bool:
    """Check that a name follows UPPER_SNAKE_CASE convention."""
    if not is_valid_identifier(name):
        return False
    return name == name.upper()


# ─────────────────────────────────────────────────────────────────────────────
# Size computation helpers
# ─────────────────────────────────────────────────────────────────────────────


def type_size(type_str: str) -> int:
    """Return the byte size of a type. Returns 0 for STRING (variable)."""
    return TYPE_SIZES.get(type_str, 0)


def min_payload_size(cmd: dict) -> int:
    """Compute the minimum expected payload size for a command
    (commandType bytes + required params only)."""
    size = 2  # commandType is always 2 bytes
    for param in cmd.get("params", []):
        if not param.get("required", True):
            continue
        t = param.get("type", "")
        if t == "STRING":
            size += 2  # typeAndSize byte + null terminator minimum
        else:
            size += type_size(t)
    return size


def max_packed_param_size(data: dict) -> int:
    """Compute the maximum packed parameter size across all commands
    and settings for a device. Used to size PackedCommand::paramBytes."""
    max_size = 0

    for cmd in data.get("commands", []):
        cmd_size = 0
        for param in cmd.get("params", []):
            t = param.get("type", "")
            if t == "STRING":
                max_len = param.get("maxLength", 15)
                cmd_size += 1 + max_len  # typeAndSize byte + data
            else:
                cmd_size += type_size(t)
        max_size = max(max_size, cmd_size)

    for setting in data.get("settings", []):
        t = setting.get("type", "")
        if t == "STRING":
            setting_size = 1 + 15  # typeAndSize + max string length
        else:
            setting_size = type_size(t)
        max_size = max(max_size, setting_size)

    return max_size


# ─────────────────────────────────────────────────────────────────────────────
# Trigger helpers
# ─────────────────────────────────────────────────────────────────────────────


def trigger_config_expression(source: dict) -> str:
    """Generate the TriggerConfig factory call for a telemetry source."""
    trigger = source.get("trigger")
    if not trigger:
        return "TriggerConfig::onChange(0.0f)"

    t = trigger.get("type", "")
    if t == "onChange":
        threshold = trigger.get("threshold", 0.0)
        return f"TriggerConfig::onChange({threshold}f)"
    if t == "periodic":
        interval_ms = trigger.get("intervalMs", 1000)
        return f"TriggerConfig::periodic({interval_ms}UL)"
    if t == "onRequest":
        return "TriggerConfig::onRequest()"

    return "TriggerConfig::onChange(0.0f)"


# ─────────────────────────────────────────────────────────────────────────────
# Optional param helpers
# ─────────────────────────────────────────────────────────────────────────────


def get_optional_param_index(cmd: dict) -> int:
    """Return the index of the optional parameter, or -1 if none."""
    for i, param in enumerate(cmd.get("params", [])):
        if not param.get("required", True):
            return i
    return -1


def format_default_value(type_str: str, default_val: str) -> str:
    """Format a default value for use in generated C++ code."""
    if type_str == "FLOAT":
        return f"{default_val}f"
    if type_str == "STRING":
        return f'"{default_val}"'
    if type_str in ("INT8", "UINT8", "INT16", "UINT16", "INT32", "UINT32"):
        cpp_type = CPP_TYPES[type_str]
        return f"{cpp_type}({default_val})"
    return default_val
