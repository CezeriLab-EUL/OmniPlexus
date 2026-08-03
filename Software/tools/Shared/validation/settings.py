from __future__ import annotations
from .result import ValidationResult
from utils import VALID_TYPES, to_hex, is_valid_identifier, is_upper_snake_case

MIN_ID = 0x0001


def validate_settings(data: dict, result: ValidationResult) -> None:
    """Validate the settings array of a device definition."""
    if "settings" not in data:
        return  # settings is optional

    if not isinstance(data["settings"], list):
        result.add_error("'settings' field must be an array")
        return

    if not data["settings"]:
        result.add_warning("'settings' array is empty — nothing to generate")
        return

    seen_ids: set[int] = set()
    seen_names: set[str] = set()

    for i, setting in enumerate(data["settings"]):
        label = f"settings[{i}]"

        # ── name ──────────────────────────────────────────────────────────────
        if (
            "name" not in setting
            or not isinstance(setting["name"], str)
            or not setting["name"]
        ):
            result.add_error(f"{label} is missing a valid 'name' field")
        else:
            name = setting["name"]

            if not is_valid_identifier(name):
                result.add_error(
                    f"Setting '{name}' has invalid name format. Names must start "
                    f"with a letter or underscore and contain only alphanumeric "
                    f"characters and underscores."
                )

            if not is_upper_snake_case(name):
                result.add_warning(
                    f"Setting '{name}' does not follow UPPER_SNAKE_CASE convention."
                )

            if name in seen_names:
                result.add_error(f"{label} has duplicate 'name' value ({name})")
            seen_names.add(name)

        # ── id ────────────────────────────────────────────────────────────────
        if (
            "id" not in setting
            or not isinstance(setting["id"], int)
            or setting["id"] < 0
        ):
            result.add_error(f"{label} is missing a valid 'id' field")
        else:
            setting_id = setting["id"]

            if setting_id < MIN_ID:
                result.add_error(
                    f"{label} has invalid 'id' value ({to_hex(setting_id)}). "
                    f"Must be >= {to_hex(MIN_ID)}"
                )

            if setting_id in seen_ids:
                result.add_error(
                    f"{label} has duplicate 'id' value ({to_hex(setting_id)})"
                )
            seen_ids.add(setting_id)

        # ── type ──────────────────────────────────────────────────────────────
        if "type" not in setting or not isinstance(setting["type"], str):
            result.add_error(f"{label} is missing the 'type' field")
        elif setting["type"] not in VALID_TYPES:
            result.add_error(
                f"{label} has unsupported type '{setting['type']}'. "
                f"Valid types: {', '.join(sorted(VALID_TYPES))}"
            )

        # ── description ───────────────────────────────────────────────────────
        if not setting.get("description"):
            result.add_warning(f"{label} is missing a 'description' field")
