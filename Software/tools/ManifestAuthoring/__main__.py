#!/usr/bin/env python3
#
# Usage:
#   python -m ManifestAuthoring <manifests_folder>
#

from __future__ import annotations
import argparse
import os
import sys

from .builder import build_manifest_interactive, review_and_write, DEFAULT_MAX_PARAMS
from .config import DEFAULT_SUMMARY_THRESHOLD


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Interactively author a new OmniPlexus device manifest.",
    )
    parser.add_argument(
        "manifests_folder",
        help="Folder where the new manifest YAML file will be written "
        "(and where existing manifests are scanned for typeShift/collision checks).",
    )
    parser.add_argument(
        "--max-params",
        type=int,
        default=DEFAULT_MAX_PARAMS,
        metavar="N",
        help="Maximum number of parameters per command (default: 3)",
    )
    parser.add_argument(
        "--summary-threshold",
        type=int,
        default=DEFAULT_SUMMARY_THRESHOLD,
        metavar="N",
        help="Show a running 'so far' summary once a command/telemetry/settings "
        "list reaches this length (default: 5)",
    )
    args = parser.parse_args()

    os.makedirs(args.manifests_folder, exist_ok=True)

    manifest = build_manifest_interactive(
        args.manifests_folder, summary_threshold=args.summary_threshold
    )
    written = review_and_write(
        manifest, args.manifests_folder, max_params=args.max_params
    )

    sys.exit(0 if written else 1)


if __name__ == "__main__":
    main()
