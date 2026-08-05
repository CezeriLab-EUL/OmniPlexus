#
# Authoring loop for a single command's params array. Enforces the
# trailing-optional-param rule live (matches validate_param_ordering in
# validation/commands.py) so a bad ordering is caught the moment it
# happens, not after the whole manifest is built.
#

from __future__ import annotations
from utils import VALID_TYPES
from prompts import (
    prompt_text,
    prompt_optional_text,
    prompt_int,
    prompt_confirm,
    prompt_select,
    confirm_shape,
)
from .identifiers import check_identifier, warn_if_not_upper_snake

DEFAULT_MAX_PARAMS = 3


def build_param(
    param_index: int, has_optional_already: bool, existing_names: set[str]
) -> dict:
    """Build and confirm a single command parameter."""
    while True:
        name = prompt_text(
            "  Param name",
            validator=lambda n: check_identifier(n) and n not in existing_names,
            error_msg="Must be a valid identifier not already used by "
            "another param on this command.",
        )
        warn_if_not_upper_snake(name, "param name")

        type_str = prompt_select("  Param type", sorted(VALID_TYPES))

        max_length = None
        if type_str == "STRING":
            max_length = prompt_int("  Max string length", min_val=1, max_val=16)

        allow_optional = not has_optional_already
        required = True
        default = None

        if allow_optional:
            required = not prompt_confirm(
                "  Is this param optional? (must be the LAST param if so)",
                default=False,
            )

        if not required:
            default = prompt_text(f"  Default value for {name}")

        description = prompt_optional_text("  Description")

        param: dict = {
            "name": name,
            "type": type_str,
            "required": required,
        }
        if max_length is not None:
            param["maxLength"] = max_length
        if default is not None:
            param["default"] = default
        if description:
            param["description"] = description

        if confirm_shape(f"Param [{param_index}]", param):
            return param
        print("  Let's redo this param.")


def build_params(max_params: int = DEFAULT_MAX_PARAMS) -> list[dict]:
    """Build the params array for a command, enforcing the trailing-optional
    rule live rather than deferring to validate()."""
    params: list[dict] = []
    has_optional = False
    existing_names: set[str] = set()

    if not prompt_confirm("Does this command take any parameters?", default=False):
        return params

    while True:
        if len(params) >= max_params:
            print(f"  Reached the {max_params}-param limit for a command.")
            break

        param = build_param(len(params), has_optional, existing_names)
        params.append(param)
        existing_names.add(param["name"])
        if not param["required"]:
            has_optional = True

        if len(params) >= max_params:
            break
        if has_optional:
            # Optional param must be last — no more params allowed after it.
            break
        if not prompt_confirm("Add another param?", default=False):
            break

    return params
