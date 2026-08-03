#
# Top-level orchestration for manifest authoring.
#

from __future__ import annotations
import os

import yaml

from validation import validate
from writer import write_file
from prompts import prompt_text, prompt_confirm, prompt_select
from display import print_header, print_success, print_error, print_yaml

from .identifiers import device_name_valid
from .type_shift import resolve_type_shift, scan_taken_type_shifts
from .commands import build_commands
from .telemetry import build_telemetry
from .settings import build_settings
from .config import DEFAULT_SUMMARY_THRESHOLD

DEFAULT_MAX_PARAMS = 5


def build_manifest_interactive(
    manifests_folder: str, summary_threshold: int = DEFAULT_SUMMARY_THRESHOLD
) -> dict:
    """Walk the user through building one complete device manifest dict.

    This is the single shared entry point that both the standalone
    manifest-authoring CLI and (later) the session-bootstrap tool should
    call — there should never be a second implementation of this flow.
    """
    print_header("OmniPlexus Manifest Authoring")

    taken_type_shifts = scan_taken_type_shifts(manifests_folder)
    if taken_type_shifts:
        print("Existing devices in this manifests folder:")
        for device in sorted(taken_type_shifts.values()):
            print(f"  {device}")
        print()

    device_name = prompt_text(
        "Device name (PascalCase, letters/digits only)",
        validator=device_name_valid,
        error_msg="Must start with an uppercase letter and contain only "
        "letters/digits (no underscores or spaces).",
    )

    type_shift = resolve_type_shift(manifests_folder, taken=taken_type_shifts)

    target = prompt_select("Target", ["embedded", "pc"])

    identity_only = prompt_confirm(
        "Is this an identity-only device? (reserves the typeShift, "
        "no commands/telemetry/settings yet)",
        default=False,
    )

    manifest: dict = {
        "device": device_name,
        "typeShift": type_shift,
        "target": target,
    }

    if identity_only:
        manifest["identityOnly"] = True
        return manifest

    manifest["commands"] = build_commands(summary_threshold=summary_threshold)

    telemetry = build_telemetry(summary_threshold=summary_threshold)
    if telemetry:
        manifest["telemetry"] = telemetry

    settings = build_settings(summary_threshold=summary_threshold)
    if settings:
        manifest["settings"] = settings

    return manifest


def review_and_write(
    manifest: dict, manifests_folder: str, max_params: int = DEFAULT_MAX_PARAMS
) -> bool:
    """Show the final manifest, confirm, validate for real, and write to disk.

    Returns True if the manifest was written, False if the user backed out
    or validation failed.
    """
    print_header("Final manifest preview")
    yaml_text = yaml.safe_dump(manifest, sort_keys=False, default_flow_style=False)
    print_yaml(yaml_text)

    if not prompt_confirm("Write this manifest to disk?", default=True):
        print("Discarded — nothing written.")
        return False

    result = validate(manifest, max_params=max_params)
    result.print_results(label=manifest["device"])

    if not result.valid:
        print_error(
            "Validation failed — manifest was NOT written. "
            "Fix the issues above and retry."
        )
        return False

    out_path = os.path.join(manifests_folder, f"{manifest['device']}.yaml")
    write_file(out_path, yaml_text)
    print_success(f"\u2713 Wrote: {out_path}")
    return True
