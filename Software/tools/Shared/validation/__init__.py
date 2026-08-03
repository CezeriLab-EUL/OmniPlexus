from .result import ValidationResult
from .manifest import validate_device_fields
from .commands import validate_commands
from .telemetry import validate_telemetry
from .settings import validate_settings
from .cross_device import validate_cross_device


def validate(data: dict, max_params: int = 3) -> ValidationResult:
    """Run all per-device validation checks on a single device definition."""
    result = ValidationResult()

    # Abort early if critical top-level fields are missing
    if not validate_device_fields(data, result):
        return result

    validate_commands(data, result, max_params=max_params)
    validate_telemetry(data, result)
    validate_settings(data, result)

    return result


def validate_all_cross_device(all_data: list[dict]) -> ValidationResult:
    """Run cross-device validation across all device definitions."""
    result = ValidationResult()
    validate_cross_device(all_data, result)
    return result


__all__ = [
    "ValidationResult",
    "validate",
    "validate_all_cross_device",
]
