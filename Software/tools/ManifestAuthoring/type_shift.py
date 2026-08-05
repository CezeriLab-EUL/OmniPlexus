#
# Scans existing manifests for taken typeShift values and interactively
# resolves one for a brand-new device manifest.
#

from __future__ import annotations
import os

import yaml

from prompts import prompt_id_with_suggestion
from display import print_table

MAX_TYPE_SHIFT = 0x1F  # 31


def find_manifest_by_device_name(manifests_folder: str, device_name: str) -> dict | None:
    """Return the parsed manifest dict whose 'device' field matches
    device_name exactly, scanning file *contents* rather than assuming
    the filename matches. A manifest file can be renamed independently
    of its device: field (nothing enforces filename == device name), so
    looking up "<device_name>.yaml" directly can silently miss a
    manifest that's actually present under a different filename."""
    if not os.path.isdir(manifests_folder):
        return None

    for fname in os.listdir(manifests_folder):
        if not (fname.endswith(".yaml") or fname.endswith(".yml")):
            continue
        path = os.path.join(manifests_folder, fname)
        try:
            with open(path, encoding="utf-8") as f:
                data = yaml.safe_load(f)
        except (yaml.YAMLError, OSError):
            continue
        if isinstance(data, dict) and data.get("device") == device_name:
            return data

    return None


def scan_taken_type_shifts(manifests_folder: str) -> dict[int, str]:
    """Return {typeShift: device_name} for every manifest already on disk."""
    taken: dict[int, str] = {}
    if not os.path.isdir(manifests_folder):
        return taken

    for fname in os.listdir(manifests_folder):
        if not (fname.endswith(".yaml") or fname.endswith(".yml")):
            continue
        path = os.path.join(manifests_folder, fname)
        try:
            with open(path, encoding="utf-8") as f:
                data = yaml.safe_load(f)
        except (yaml.YAMLError, OSError):
            continue
        if not isinstance(data, dict):
            continue
        if "typeShift" in data and isinstance(data["typeShift"], int):
            taken[data["typeShift"]] = data.get("device", fname)

    return taken


def suggest_free_type_shift(taken: dict[int, str]) -> int:
    for candidate in range(0, MAX_TYPE_SHIFT + 1):
        if candidate not in taken:
            return candidate
    raise RuntimeError("All 32 typeShift values (0-31) are already in use.")


def resolve_type_shift(manifests_folder: str, taken: dict[int, str] | None = None) -> int:
    """Interactively resolve a typeShift for a brand-new device manifest.

    Scans existing manifests for taken values, suggests the lowest free
    one, and rejects any user-entered value that collides. If `taken` is
    already known (e.g. the caller scanned it earlier to show existing
    device names), pass it in to avoid re-scanning the folder.
    """
    if taken is None:
        taken = scan_taken_type_shifts(manifests_folder)

    if taken:
        print_table(
            "Existing typeShift values",
            ["typeShift", "Device"],
            sorted(taken.items()),
        )

    suggestion = suggest_free_type_shift(taken)
    return prompt_id_with_suggestion(
        "typeShift", suggestion, taken=taken, min_val=0, max_val=MAX_TYPE_SHIFT
    )
