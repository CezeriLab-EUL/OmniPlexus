#
# Authoring loop for the manifest's optional `settings` array (mirrors the
# rules in validation/settings.py — same shape as telemetry, minus trigger).
#

from __future__ import annotations
from utils import VALID_TYPES
from prompts import (
    prompt_text,
    prompt_optional_text,
    prompt_confirm,
    prompt_select,
    prompt_id_with_suggestion,
    confirm_shape,
)
from display import print_running_list
from .identifiers import check_identifier, warn_if_not_upper_snake
from .config import DEFAULT_SUMMARY_THRESHOLD

MIN_ID = 1


def _build_setting(existing_ids: set[int], existing_names: set[str]) -> dict:
    while True:
        name = prompt_text(
            "Setting name",
            validator=lambda n: check_identifier(n) and n not in existing_names,
            error_msg="Must be a valid, unique identifier not already used "
            "by another setting on this device.",
        )
        warn_if_not_upper_snake(name, "setting name")

        suggestion = 1
        while suggestion in existing_ids:
            suggestion += 1
        setting_id = prompt_id_with_suggestion(
            "Setting id", suggestion, taken=existing_ids, min_val=MIN_ID
        )

        type_str = prompt_select("Value type", sorted(VALID_TYPES))
        description = prompt_optional_text("Description")

        setting: dict = {"name": name, "id": setting_id, "type": type_str}
        if description:
            setting["description"] = description

        if confirm_shape("Setting", setting):
            return setting
        print("Let's redo this setting.")


def build_settings(summary_threshold: int = DEFAULT_SUMMARY_THRESHOLD) -> list[dict]:
    settings: list[dict] = []
    if not prompt_confirm("\nAdd any settings for this device?", default=False):
        return settings

    existing_ids: set[int] = set()
    existing_names: set[str] = set()

    while True:
        setting = _build_setting(existing_ids, existing_names)
        settings.append(setting)
        existing_ids.add(setting["id"])
        existing_names.add(setting["name"])

        if len(settings) >= summary_threshold:
            print_running_list("Settings so far", [s["name"] for s in settings])
        if not prompt_confirm("Add another setting?", default=False):
            break
        print()

    return settings
