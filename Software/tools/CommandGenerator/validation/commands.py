from __future__ import annotations
from .result import ValidationResult
from utils import VALID_TYPES, to_hex, is_valid_identifier, is_upper_snake_case

MIN_ID = 0x0001


def validate_param(
    param: dict,
    cmd_name: str,
    param_index: int,
    result: ValidationResult,
) -> None:
    """Validate a single command parameter."""

    # ── name ──────────────────────────────────────────────────────────────────
    if "name" not in param or not isinstance(param["name"], str) or not param["name"]:
        result.add_error(
            f"Command '{cmd_name}' param[{param_index}] is missing a valid 'name' field"
        )
    param_name = param.get("name") or f"[param_{param_index}]"

    # ── type ──────────────────────────────────────────────────────────────────
    if "type" not in param or not isinstance(param["type"], str):
        result.add_error(
            f"Command '{cmd_name}' param '{param_name}' is missing the 'type' field"
        )
    else:
        if param["type"] not in VALID_TYPES:
            result.add_error(
                f"Command '{cmd_name}' param '{param_name}' has unsupported type "
                f"'{param['type']}'. Valid types: {', '.join(sorted(VALID_TYPES))}"
            )

    # ── required ──────────────────────────────────────────────────────────────
    if "required" not in param or not isinstance(param["required"], bool):
        result.add_error(
            f"Command '{cmd_name}' param '{param_name}' is missing the 'required' "
            f"field (must be true or false)"
        )

    # ── description ───────────────────────────────────────────────────────────
    if not param.get("description"):
        result.add_warning(
            f"Command '{cmd_name}' param '{param_name}' is missing a 'description'"
        )

    # ── STRING-specific ───────────────────────────────────────────────────────
    if param.get("type") == "STRING":
        if (
            "maxLength" not in param
            or not isinstance(param["maxLength"], int)
            or param["maxLength"] <= 0
        ):
            result.add_error(
                f"Command '{cmd_name}' param '{param_name}' is missing a valid "
                f"'maxLength' field (must be a positive integer)"
            )

        if not param.get("required", True) and "default" in param:
            default_val = param["default"]
            max_len = param.get("maxLength", 0)
            if isinstance(default_val, str) and len(default_val) > max_len:
                result.add_error(
                    f"Command '{cmd_name}' param '{param_name}' default value "
                    f"'{default_val}' exceeds maxLength ({max_len})"
                )


def validate_param_ordering(
    params: list,
    cmd_name: str,
    result: ValidationResult,
    max_optional: int = 1,
) -> None:
    """Validate that optional params come last and there's at most max_optional of them."""
    found_optional = False
    optional_count = 0

    for i, param in enumerate(params):
        is_required = param.get("required", True)
        param_name = param.get("name") or f"[param_{i}]"

        if not is_required:
            optional_count += 1
            found_optional = True

            if i != len(params) - 1:
                result.add_error(
                    f"Command '{cmd_name}' has optional parameter '{param_name}' "
                    f"that is not the last parameter. Optional parameters must be last."
                )

            if not param.get("default"):
                result.add_error(
                    f"Command '{cmd_name}' has optional parameter '{param_name}' "
                    f"without a 'default' value. All optional parameters must have defaults."
                )
        else:
            if found_optional:
                result.add_error(
                    f"Command '{cmd_name}' has required parameter '{param_name}' "
                    f"after an optional parameter. Required parameters must come first."
                )

    if optional_count > max_optional:
        result.add_error(
            f"Command '{cmd_name}' has {optional_count} optional parameters. "
            f"Maximum is {max_optional}."
        )


def validate_commands(
    data: dict,
    result: ValidationResult,
    max_params: int = 3,
    max_optional: int = 1,
) -> None:
    """Validate all commands in a device definition."""
    seen_ids: set[int] = set()
    seen_names: set[str] = set()

    for i, cmd in enumerate(data.get("commands", [])):
        label = f"commands[{i}]"

        # ── name ──────────────────────────────────────────────────────────────
        if "name" not in cmd or not isinstance(cmd["name"], str) or not cmd["name"]:
            result.add_error(f"{label} is missing a valid 'name' field")
            continue

        name = cmd["name"]

        if not is_valid_identifier(name):
            result.add_error(
                f"Command '{name}' has invalid name format. Names must start with "
                f"a letter or underscore and contain only alphanumeric characters "
                f"and underscores."
            )

        if not is_upper_snake_case(name):
            result.add_warning(
                f"Command '{name}' does not follow UPPER_SNAKE_CASE convention."
            )

        if name in seen_names:
            result.add_error(f"{label} has duplicate 'name' value ({name})")
        seen_names.add(name)

        # ── acknowledges ──────────────────────────────────────────────────────
        if "acknowledges" not in cmd or not isinstance(cmd["acknowledges"], bool):
            result.add_error(f"{label} is missing a valid 'acknowledges' field")

        # ── id ────────────────────────────────────────────────────────────────
        if "id" not in cmd or not isinstance(cmd["id"], int) or cmd["id"] < 0:
            result.add_error(f"{label} is missing a valid 'id' field")
            continue

        cmd_id = cmd["id"]

        if cmd_id < MIN_ID:
            result.add_error(
                f"{label} has invalid 'id' value ({to_hex(cmd_id)}). "
                f"Must be >= {to_hex(MIN_ID)}"
            )

        if cmd_id in seen_ids:
            result.add_error(f"{label} has duplicate 'id' value ({to_hex(cmd_id)})")
        seen_ids.add(cmd_id)

        # ── params ────────────────────────────────────────────────────────────
        if "params" not in cmd or not isinstance(cmd["params"], list):
            result.add_error(f"{label} is missing a valid 'params' array")
            continue

        if len(cmd["params"]) > max_params:
            result.add_error(
                f"{label} has {len(cmd['params'])} params. " f"Maximum is {max_params}."
            )

        for j, param in enumerate(cmd["params"]):
            validate_param(param, name, j, result)

        validate_param_ordering(cmd["params"], name, result, max_optional)

        # ── description ───────────────────────────────────────────────────────
        if not cmd.get("description"):
            result.add_warning(f"{label} is missing a 'description' field")
