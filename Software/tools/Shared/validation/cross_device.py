from __future__ import annotations
from .result import ValidationResult
from utils import (
    to_hex,
    build_command_id,
    CATEGORY_COMMAND,
    CATEGORY_TELEMETRY,
    CATEGORY_SETTING_GET,
    CATEGORY_SETTING_SET,
)


def validate_cross_device(all_data: list[dict], result: ValidationResult) -> None:
    """Validate across all devices for typeShift and ID collisions."""

    # ── device name uniqueness ────────────────────────────────────────────────
    # Without this check, two manifests sharing a device name pass every
    # other check cleanly, but generate.py's per-device output files are
    # keyed by device name in a plain dict — the second device's generated
    # controller/registration files silently overwrite the first's, with
    # no error or warning anywhere.
    seen_device_names: set[str] = set()

    for data in all_data:
        if "device" not in data:
            continue
        device_name = data["device"]

        if device_name in seen_device_names:
            result.add_error(
                f"Duplicate device name '{device_name}' — two manifests "
                f"both declare this device. Each manifest must have a "
                f"unique 'device' name."
            )
        seen_device_names.add(device_name)

    # ── typeShift uniqueness ──────────────────────────────────────────────────
    seen_shifts: dict[int, str] = {}

    for data in all_data:
        if "device" not in data or "typeShift" not in data:
            continue
        device_name = data["device"]
        shift = data["typeShift"]

        if shift in seen_shifts:
            result.add_error(
                f"Duplicate 'typeShift' value {shift}: device '{device_name}' "
                f"conflicts with '{seen_shifts[shift]}'"
            )
        else:
            seen_shifts[shift] = device_name

    # ── command ID collisions ─────────────────────────────────────────────────
    seen_command_ids: dict[int, str] = {}
    seen_telemetry_ids: dict[int, str] = {}

    for data in all_data:
        if "device" not in data or "typeShift" not in data:
            continue
        device_name = data["device"]
        shift = data["typeShift"]

        # Regular commands
        for cmd in data.get("commands", []):
            if "id" not in cmd or "name" not in cmd:
                continue
            built_id = build_command_id(shift, CATEGORY_COMMAND, cmd["id"])
            label = f"{device_name}::{cmd['name']}"

            if built_id in seen_command_ids:
                result.add_error(
                    f"Cross-device command ID collision at {to_hex(built_id)}: "
                    f"'{label}' conflicts with '{seen_command_ids[built_id]}'"
                )
            else:
                seen_command_ids[built_id] = label

        # Telemetry sources
        for src in data.get("telemetry", []):
            if "id" not in src or "name" not in src:
                continue
            built_id = build_command_id(shift, CATEGORY_TELEMETRY, src["id"])
            label = f"{device_name}::{src['name']}"

            if built_id in seen_telemetry_ids:
                result.add_error(
                    f"Cross-device telemetry ID collision at {to_hex(built_id)}: "
                    f"'{label}' conflicts with '{seen_telemetry_ids[built_id]}'"
                )
            else:
                seen_telemetry_ids[built_id] = label

        # Settings GET/SET commands
        for setting in data.get("settings", []):
            if "id" not in setting or "name" not in setting:
                continue
            label = f"{device_name}::{setting['name']}"

            get_id = build_command_id(shift, CATEGORY_SETTING_GET, setting["id"])
            set_id = build_command_id(shift, CATEGORY_SETTING_SET, setting["id"])

            if get_id in seen_command_ids:
                result.add_error(
                    f"Setting GET ID collision at {to_hex(get_id)}: "
                    f"'{label}' conflicts with '{seen_command_ids[get_id]}'"
                )
            else:
                seen_command_ids[get_id] = f"{label} (GET)"

            if set_id in seen_command_ids:
                result.add_error(
                    f"Setting SET ID collision at {to_hex(set_id)}: "
                    f"'{label}' conflicts with '{seen_command_ids[set_id]}'"
                )
            else:
                seen_command_ids[set_id] = f"{label} (SET)"
