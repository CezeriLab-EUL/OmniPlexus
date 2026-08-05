#!/usr/bin/env python3
#
# Usage:
#   python -m SessionBootstrap <manifests_folder> <stubs_folder>
#

from __future__ import annotations
import argparse
import sys

from .builder import build_stub_config_interactive, review_and_write
from .config import DEFAULT_SUMMARY_THRESHOLD


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Interactively generate an OmniPlexus session-bootstrap stub.",
    )
    parser.add_argument(
        "manifests_folder",
        help="Folder to scan/author device manifests in (shared with ManifestAuthoring).",
    )
    parser.add_argument(
        "stubs_folder",
        help="Folder generated stubs are written into, as <stubs_folder>/<NodeName>/<NodeName>.ino",
    )
    parser.add_argument(
        "--summary-threshold",
        type=int,
        default=DEFAULT_SUMMARY_THRESHOLD,
        metavar="N",
        help="Show a running 'so far' summary once the transport list reaches this length (default: 3)",
    )
    args = parser.parse_args()

    config = build_stub_config_interactive(
        args.manifests_folder, summary_threshold=args.summary_threshold
    )
    written = review_and_write(config, args.stubs_folder)

    sys.exit(0 if written else 1)


if __name__ == "__main__":
    main()
