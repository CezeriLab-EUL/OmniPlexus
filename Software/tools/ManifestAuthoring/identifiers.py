#
# Name/identifier rules that mirror validation/manifest.py, commands.py,
# telemetry.py, and settings.py — checked here inline so the wizard can
# reject and re-prompt immediately, rather than only finding out after the
# whole manifest is built and run through the real validate().
#

from __future__ import annotations
from utils import is_valid_identifier, is_upper_snake_case
from display import print_note


def check_identifier(name: str) -> bool:
    return is_valid_identifier(name)


def warn_if_not_upper_snake(name: str, kind: str) -> None:
    if not is_upper_snake_case(name):
        print_note(
            f"'{name}' doesn't follow UPPER_SNAKE_CASE convention. "
            f"This is only a warning — the {kind} will still work."
        )


def device_name_valid(name: str) -> bool:
    """Mirrors validate_device_fields' device-name rule: must start
    uppercase and be alphanumeric only (no underscores or spaces)."""
    return name[0:1].isupper() and name.isalnum()
