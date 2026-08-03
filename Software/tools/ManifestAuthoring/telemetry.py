#
# Authoring loop for the manifest's optional `telemetry` array, including
# the onChange/periodic/onRequest trigger config (mirrors the rules in
# validation/telemetry.py).
#

from __future__ import annotations
from utils import VALID_TYPES
from prompts import (
    prompt_text,
    prompt_optional_text,
    prompt_optional_float,
    prompt_int,
    prompt_confirm,
    prompt_select,
    prompt_id_with_suggestion,
    confirm_shape,
)
from display import print_running_list
from .identifiers import check_identifier, warn_if_not_upper_snake
from .config import DEFAULT_SUMMARY_THRESHOLD

MIN_ID = 1


def _build_trigger() -> dict | None:
    if not prompt_confirm("Add a trigger config for this telemetry source?", default=False):
        return None

    trigger_type = prompt_select("Trigger type", ["onChange", "periodic", "onRequest"])

    if trigger_type == "onChange":
        threshold = prompt_optional_float("  Threshold")
        trigger = {"type": "onChange"}
        if threshold is not None:
            trigger["threshold"] = threshold
        return trigger

    if trigger_type == "periodic":
        interval = prompt_int("  Interval in ms (10-65535)", min_val=10, max_val=65535)
        return {"type": "periodic", "intervalMs": interval}

    return {"type": "onRequest"}


def _build_telemetry_source(existing_ids: set[int], existing_names: set[str]) -> dict:
    while True:
        name = prompt_text(
            "Telemetry source name",
            validator=lambda n: check_identifier(n) and n not in existing_names,
            error_msg="Must be a valid, unique identifier not already used "
            "by another telemetry source on this device.",
        )
        warn_if_not_upper_snake(name, "telemetry source name")

        suggestion = 1
        while suggestion in existing_ids:
            suggestion += 1
        src_id = prompt_id_with_suggestion(
            "Source id", suggestion, taken=existing_ids, min_val=MIN_ID
        )

        type_str = prompt_select("Value type", sorted(VALID_TYPES))
        description = prompt_optional_text("Description")
        trigger = _build_trigger()

        source: dict = {"name": name, "id": src_id, "type": type_str}
        if description:
            source["description"] = description
        if trigger:
            source["trigger"] = trigger

        if confirm_shape("Telemetry source", source):
            return source
        print("Let's redo this telemetry source.")


def build_telemetry(summary_threshold: int = DEFAULT_SUMMARY_THRESHOLD) -> list[dict]:
    telemetry: list[dict] = []
    if not prompt_confirm("\nAdd any telemetry sources for this device?", default=False):
        return telemetry

    existing_ids: set[int] = set()
    existing_names: set[str] = set()

    while True:
        source = _build_telemetry_source(existing_ids, existing_names)
        telemetry.append(source)
        existing_ids.add(source["id"])
        existing_names.add(source["name"])

        if len(telemetry) >= summary_threshold:
            print_running_list("Telemetry sources so far", [s["name"] for s in telemetry])
        if not prompt_confirm("Add another telemetry source?", default=False):
            break
        print()

    return telemetry