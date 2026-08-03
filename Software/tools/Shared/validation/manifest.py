from __future__ import annotations
from .result import ValidationResult
from utils import is_valid_identifier


def validate_device_fields(data: dict, result: ValidationResult) -> bool:
    """Validate the top-level device name and typeShift fields.
    Returns False if validation should be aborted (critical fields missing)."""

    # ── device name ───────────────────────────────────────────────────────────
    if (
        "device" not in data
        or not isinstance(data["device"], str)
        or not data["device"]
    ):
        result.add_error("JSON is missing the top-level 'device' field")
        return False

    device_name = data["device"]

    if not device_name[0].isupper():
        result.add_error(
            f"Device name '{device_name}' should start with an uppercase "
            f"letter (PascalCase)"
        )

    if not device_name.isalnum():
        result.add_error(
            f"'device' field must contain only alphanumeric characters "
            f"(no spaces or underscores)"
        )

    # ── typeShift ─────────────────────────────────────────────────────────────
    if "typeShift" not in data:
        result.add_error(
            "'typeShift' field is missing. Must be an unsigned integer "
            "between 0 and 31"
        )
    elif not isinstance(data["typeShift"], int) or data["typeShift"] < 0:
        result.add_error("'typeShift' must be a non-negative integer")
    elif data["typeShift"] > 0x1F:
        result.add_error(
            f"'typeShift' value {data['typeShift']} is out of range. "
            f"Must be between 0 and 31"
        )

    # ── target ────────────────────────────────────────────────────────────────
    valid_targets = ("embedded", "pc")
    if "target" not in data:
        result.add_error(
            f"'target' field is missing. Must be one of: {', '.join(valid_targets)}"
        )
        return False

    if data["target"] not in valid_targets:
        result.add_error(
            f"'target' value '{data['target']}' is invalid. "
            f"Must be one of: {', '.join(valid_targets)}"
        )
        return False

    # ── identityOnly ──────────────────────────────────────────────────────────
    identity_only = data.get("identityOnly", False)

    if not isinstance(identity_only, bool):
        result.add_error("'identityOnly' field must be a boolean")
        return False

    # ── commands array presence ───────────────────────────────────────────────
    if identity_only:
        # Identity-only manifests deliberately have no commands — they exist
        # solely to reserve a typeShift. Skip the commands requirement.
        return True

    if "commands" not in data:
        result.add_error("JSON is missing the top-level 'commands' array")
        return False

    if not isinstance(data["commands"], list):
        result.add_error("'commands' field must be an array")
        return False

    if not data["commands"]:
        result.add_error("'commands' array is empty — nothing to generate")
        return False

    return True
