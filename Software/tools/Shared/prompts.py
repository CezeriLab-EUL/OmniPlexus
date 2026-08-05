#
# Interactive prompting primitives shared by ManifestAuthoring and (later)
# the session-bootstrap tool, built on questionary. This replaces the old
# input()-based io_helpers.py — the same set of operations (text entry
# with inline validation, yes/no with Enter-to-accept-default, single
# choice from a list, an ID prompt with a pre-computed suggestion) but
# with arrow-key selection and inline re-editable validation instead of
# reject-and-reprint-the-whole-question.
#
# NOTE: every function here calls .ask() and treats a None result (the
# user pressed Ctrl+C / Esc) as a deliberate cancellation, raising
# KeyboardInterrupt so callers don't have to check for None everywhere.
#

from __future__ import annotations
import questionary
import yaml

from display import print_header, print_yaml


def _unwrap(result):
    if result is None:
        raise KeyboardInterrupt("Prompt cancelled")
    return result


def prompt_text(
    label: str,
    validator=None,
    error_msg: str = "Invalid value.",
) -> str:
    """Prompt for a non-empty string. Invalid input is rejected inline
    (the same prompt stays open with an error shown) rather than
    reprinting the question."""

    def _validate(text: str) -> bool | str:
        text = text.strip()
        if not text:
            return "This field can't be empty."
        if validator and not validator(text):
            return error_msg
        return True

    result = questionary.text(f"{label}:", validate=_validate).ask()
    return _unwrap(result).strip()


def prompt_optional_text(label: str) -> str:
    """Prompt for a string that may be left blank."""
    result = questionary.text(f"{label} (optional, press Enter to skip):").ask()
    return _unwrap(result).strip()


def prompt_int(
    label: str,
    min_val: int | None = None,
    max_val: int | None = None,
) -> int:
    """Prompt for an integer within an optional range."""

    def _validate(text: str) -> bool | str:
        text = text.strip()
        try:
            value = int(text)
        except ValueError:
            return "Please enter a whole number."
        if min_val is not None and value < min_val:
            return f"Must be >= {min_val}."
        if max_val is not None and value > max_val:
            return f"Must be <= {max_val}."
        return True

    result = questionary.text(f"{label}:", validate=_validate).ask()
    return int(_unwrap(result).strip())


def prompt_int_with_default(
    label: str,
    default: int,
    min_val: int | None = None,
    max_val: int | None = None,
) -> int:
    """Prompt for an integer with a pre-filled default; Enter accepts it.
    Same Enter-to-accept convention as prompt_id_with_suggestion, for
    non-ID integer fields (e.g. reconnect attempts, stack size)."""

    def _validate(text: str) -> bool | str:
        text = text.strip()
        if text == "":
            return True
        try:
            value = int(text)
        except ValueError:
            return "Please enter a whole number."
        if min_val is not None and value < min_val:
            return f"Must be >= {min_val}."
        if max_val is not None and value > max_val:
            return f"Must be <= {max_val}."
        return True

    result = questionary.text(f"{label} [default: {default}]:", validate=_validate).ask()
    result = _unwrap(result).strip()
    return default if result == "" else int(result)


def prompt_confirm(label: str, default: bool = True) -> bool:
    """Prompt for a yes/no answer. Enter accepts the default —
    questionary.confirm supports this natively."""
    result = questionary.confirm(label, default=default).ask()
    return _unwrap(result)


def prompt_select(label: str, options: list[str]) -> str:
    """Prompt the user to pick one of a fixed list of options via an
    arrow-key menu (replaces the old type-the-word-and-hope-it-matches
    prompt_choice)."""
    result = questionary.select(label, choices=options).ask()
    return _unwrap(result)


def prompt_id_with_suggestion(
    label: str,
    suggestion: int,
    taken: dict[int, str] | set[int] | None = None,
    min_val: int = 1,
    max_val: int | None = None,
) -> int:
    """Prompt for an integer ID with a pre-computed suggestion.

    Enter accepts the suggestion. A typed value is validated inline
    against min_val/max_val and rejected if already present in `taken`
    (a dict of {id: owner_label} gives a more descriptive collision
    message; a plain set of ids also works).

    This consolidates the "suggested value, Enter to accept, reject
    collisions" pattern that was previously duplicated across
    commands.py, telemetry.py, settings.py, and type_shift.py.
    """
    taken = taken or {}
    is_dict = isinstance(taken, dict)

    def _validate(text: str) -> bool | str:
        text = text.strip()
        if text == "":
            return True  # empty means "use the suggestion"
        try:
            value = int(text)
        except ValueError:
            return "Please enter a whole number."
        if value < min_val:
            return f"Must be >= {min_val}."
        if max_val is not None and value > max_val:
            return f"Must be <= {max_val}."
        if value in taken:
            if is_dict:
                return f"{value} is already used by '{taken[value]}'. Pick another."
            return f"{value} is already used. Pick another."
        return True

    prompt_label = f"{label} [suggested: {suggestion}, press Enter to accept]:"
    result = questionary.text(prompt_label, validate=_validate).ask()
    result = _unwrap(result).strip()
    return suggestion if result == "" else int(result)


def prompt_optional_float(label: str) -> float | None:
    """Prompt for an optional float; blank input returns None. This replaces
    telemetry.py's previous plain input()+float() call, which had no
    validation and would crash on non-numeric input."""

    def _validate(text: str) -> bool | str:
        text = text.strip()
        if text == "":
            return True
        try:
            float(text)
        except ValueError:
            return "Please enter a number (or leave blank to skip)."
        return True

    result = questionary.text(f"{label} (optional, Enter to skip):", validate=_validate).ask()
    result = _unwrap(result).strip()
    return float(result) if result else None


def prompt_multi_select(
    label: str, options: list[str], default_selected: list[str] | None = None
) -> list[str]:
    """Prompt the user to pick zero or more options via a checkbox menu
    (space to toggle, Enter to confirm). default_selected pre-checks the
    given option values."""
    default_selected = default_selected or []
    choices = [
        questionary.Choice(title=opt, checked=(opt in default_selected))
        for opt in options
    ]
    result = questionary.checkbox(label, choices=choices).ask()
    return _unwrap(result)


def confirm_shape(label: str, shape: dict) -> bool:
    """Show a headered YAML preview of a built dict — reflecting exactly
    what will end up in the manifest — and ask the user to confirm it."""
    print_header(label)
    yaml_text = yaml.safe_dump(shape, sort_keys=False, default_flow_style=False)
    print_yaml(yaml_text)
    return prompt_confirm("Is this correct?", default=True)
