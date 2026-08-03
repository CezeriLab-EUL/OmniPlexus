#
# Authoring loop for the manifest's `commands` array. At least one command
# is required by validation/manifest.py (unless identityOnly), so this
# loop runs a minimum of once.
#

from __future__ import annotations
from prompts import (
    prompt_text,
    prompt_optional_text,
    prompt_confirm,
    prompt_id_with_suggestion,
    confirm_shape,
)
from display import print_running_list
from .identifiers import check_identifier, warn_if_not_upper_snake
from .params import build_params
from .config import DEFAULT_SUMMARY_THRESHOLD

MIN_ID = 1


def _build_command(existing_ids: set[int], existing_names: set[str]) -> dict:
    while True:
        name = prompt_text(
            "Command name",
            validator=lambda n: check_identifier(n) and n not in existing_names,
            error_msg="Must be a valid, unique identifier not already used "
            "by another command on this device.",
        )
        warn_if_not_upper_snake(name, "command name")

        suggestion = 1
        while suggestion in existing_ids:
            suggestion += 1
        cmd_id = prompt_id_with_suggestion(
            "Command id", suggestion, taken=existing_ids, min_val=MIN_ID
        )

        acknowledges = prompt_confirm(
            "Does this command expect an acknowledgement?", default=True
        )
        description = prompt_optional_text("Description")
        params = build_params()

        command: dict = {
            "name": name,
            "id": cmd_id,
            "acknowledges": acknowledges,
            "params": params,
        }
        if description:
            command["description"] = description

        if confirm_shape("Command", command):
            return command
        print("Let's redo this command.")


def build_commands(summary_threshold: int = DEFAULT_SUMMARY_THRESHOLD) -> list[dict]:
    commands: list[dict] = []
    existing_ids: set[int] = set()
    existing_names: set[str] = set()

    print("\n--- Commands (at least one is required) ---")
    while True:
        cmd = _build_command(existing_ids, existing_names)
        commands.append(cmd)
        existing_ids.add(cmd["id"])
        existing_names.add(cmd["name"])

        if len(commands) >= summary_threshold:
            print_running_list("Commands so far", [c["name"] for c in commands])
        if not prompt_confirm("Add another command?", default=False):
            break
        print()

    return commands
