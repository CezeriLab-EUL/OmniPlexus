from __future__ import annotations
from .result import ValidationResult
from utils import VALID_TYPES, to_hex, is_valid_identifier, is_upper_snake_case

MIN_ID = 0x0001


def validate_trigger(trigger: dict, label: str, result: ValidationResult) -> None:
    """Validate a telemetry trigger object."""
    if not isinstance(trigger, dict):
        result.add_error(f"{label} 'trigger' field must be a JSON object")
        return

    if "type" not in trigger or not isinstance(trigger["type"], str):
        result.add_error(f"{label} 'trigger' is missing the 'type' field")
        return

    trigger_type = trigger["type"]
    valid_trigger_types = {"onChange", "periodic", "onRequest"}

    if trigger_type not in valid_trigger_types:
        result.add_error(
            f"{label} 'trigger.type' has invalid value '{trigger_type}'. "
            f"Must be one of: {', '.join(sorted(valid_trigger_types))}"
        )
        return

    if trigger_type == "periodic":
        if "intervalMs" not in trigger:
            result.add_error(
                f"{label} trigger type is 'periodic' but 'intervalMs' is missing"
            )
        elif not isinstance(trigger["intervalMs"], int) or trigger["intervalMs"] < 0:
            result.add_error(f"{label} 'trigger.intervalMs' must be a positive integer")
        else:
            interval = trigger["intervalMs"]
            if interval < 10:
                result.add_error(
                    f"{label} 'trigger.intervalMs' must be at least 10ms (got {interval})"
                )
            if interval > 65535:
                result.add_error(
                    f"{label} 'trigger.intervalMs' must be <= 65535ms (got {interval})"
                )
        if "threshold" in trigger:
            result.add_error(
                f"{label} 'trigger.threshold' is not valid for 'periodic' triggers"
            )

    if trigger_type == "onChange":
        if "threshold" in trigger:
            if not isinstance(trigger["threshold"], (int, float)):
                result.add_error(f"{label} 'trigger.threshold' must be a number")
            elif trigger["threshold"] < 0:
                result.add_error(f"{label} 'trigger.threshold' must be >= 0")
        if "intervalMs" in trigger:
            result.add_error(
                f"{label} 'trigger.intervalMs' is not valid for 'onChange' triggers"
            )

    if trigger_type == "onRequest":
        if "intervalMs" in trigger:
            result.add_error(
                f"{label} 'trigger.intervalMs' is not valid for 'onRequest' triggers"
            )
        if "threshold" in trigger:
            result.add_error(
                f"{label} 'trigger.threshold' is not valid for 'onRequest' triggers"
            )


def validate_telemetry_source(
    source: dict, index: int, result: ValidationResult
) -> None:
    """Validate a single telemetry source entry."""
    label = f"telemetry[{index}]"

    # ── name ──────────────────────────────────────────────────────────────────
    if (
        "name" not in source
        or not isinstance(source["name"], str)
        or not source["name"]
    ):
        result.add_error(f"{label} is missing a valid 'name' field")
    else:
        name = source["name"]
        if not is_valid_identifier(name):
            result.add_error(
                f"Telemetry source '{name}' has invalid name format. Names must "
                f"start with a letter or underscore and contain only alphanumeric "
                f"characters and underscores."
            )
        if not is_upper_snake_case(name):
            result.add_warning(
                f"Telemetry source '{name}' does not follow UPPER_SNAKE_CASE convention."
            )

    # ── id ────────────────────────────────────────────────────────────────────
    if "id" not in source or not isinstance(source["id"], int) or source["id"] < 0:
        result.add_error(f"{label} is missing a valid 'id' field")
    else:
        if source["id"] < MIN_ID:
            result.add_error(
                f"{label} has invalid 'id' value ({to_hex(source['id'])}). "
                f"Must be >= {to_hex(MIN_ID)}"
            )

    # ── type ──────────────────────────────────────────────────────────────────
    if "type" not in source or not isinstance(source["type"], str):
        result.add_error(f"{label} is missing the 'type' field")
    elif source["type"] not in VALID_TYPES:
        result.add_error(
            f"{label} has unsupported type '{source['type']}'. "
            f"Valid types: {', '.join(sorted(VALID_TYPES))}"
        )

    # ── description ───────────────────────────────────────────────────────────
    if not source.get("description"):
        result.add_warning(f"{label} is missing a 'description' field")

    # ── trigger ───────────────────────────────────────────────────────────────
    if "trigger" in source:
        validate_trigger(source["trigger"], label, result)


def validate_telemetry(data: dict, result: ValidationResult) -> None:
    """Validate the telemetry array of a device definition."""
    if "telemetry" not in data:
        return  # telemetry is optional

    if not isinstance(data["telemetry"], list):
        result.add_error("'telemetry' field must be an array")
        return

    if not data["telemetry"]:
        result.add_warning("'telemetry' array is empty — nothing to generate")
        return

    seen_ids: set[int] = set()
    seen_names: set[str] = set()

    for i, source in enumerate(data["telemetry"]):
        validate_telemetry_source(source, i, result)

        if isinstance(source.get("id"), int):
            src_id = source["id"]
            if src_id in seen_ids:
                result.add_error(
                    f"telemetry[{i}] has duplicate 'id' value ({to_hex(src_id)})"
                )
            seen_ids.add(src_id)

        if isinstance(source.get("name"), str) and source["name"]:
            if source["name"] in seen_names:
                result.add_error(
                    f"telemetry[{i}] has duplicate 'name' value ({source['name']})"
                )
            seen_names.add(source["name"])
