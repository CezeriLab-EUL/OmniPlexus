#!/usr/bin/env python3
#
# sync_arduino.py
# OmniPlexus — Sync library files to Arduino libraries folder
#
# Usage:
#   python sync_arduino.py
#   python sync_arduino.py --library-path /custom/path/to/Arduino/libraries/Opx/src
#   python sync_arduino.py --dry-run
#

from __future__ import annotations
import argparse
import os
import platform
import shutil
import sys

LIBRARY_NAME = "Opx"

# ─────────────────────────────────────────────────────────────────────────────
# Platform detection
# ─────────────────────────────────────────────────────────────────────────────

def default_arduino_library_path() -> str | None:
    """Return the default Arduino libraries path for the current platform."""
    system = platform.system()

    if system == "Linux":
        return os.path.expanduser(f"~/Arduino/libraries/{LIBRARY_NAME}/src")
    elif system == "Darwin":  # macOS
        return os.path.expanduser(
            f"~/Documents/Arduino/libraries/{LIBRARY_NAME}/src"
        )
    elif system == "Windows":
        documents = os.path.join(os.path.expanduser("~"), "Documents")
        return os.path.join(documents, "Arduino", "libraries", LIBRARY_NAME, "src")
    else:
        return None


# ─────────────────────────────────────────────────────────────────────────────
# Sync helpers
# ─────────────────────────────────────────────────────────────────────────────

def sync_directory(src: str, dst: str, dry_run: bool) -> None:
    """Copy src directory to dst, replacing dst if it exists."""
    if not os.path.isdir(src):
        print(f"  ⚠  Skipping (not found): {src}")
        return

    print(f"  {'[dry-run] ' if dry_run else ''}Syncing: {src}")
    print(f"       → {dst}")

    if not dry_run:
        if os.path.exists(dst):
            shutil.rmtree(dst)
        shutil.copytree(src, dst)


def sync_file(src: str, dst: str, dry_run: bool) -> None:
    """Copy a single file to dst."""
    if not os.path.isfile(src):
        print(f"  ⚠  Skipping (not found): {src}")
        return

    print(f"  {'[dry-run] ' if dry_run else ''}Syncing: {src}")
    print(f"       → {dst}")

    if not dry_run:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)


# ─────────────────────────────────────────────────────────────────────────────
# Main sync
# ─────────────────────────────────────────────────────────────────────────────

def sync(repo_root: str, library_path: str, dry_run: bool) -> None:
    """Sync all OmniPlexus library files to the Arduino library path."""

    print(f"\nSyncing OmniPlexus to Arduino library...")
    print(f"  Source:      {repo_root}")
    print(f"  Destination: {library_path}")
    if dry_run:
        print(f"  Mode:        DRY RUN (no files will be written)\n")
    else:
        print()

    if not dry_run:
        os.makedirs(library_path, exist_ok=True)

    # ── Library headers ───────────────────────────────────────────────────────
    sync_directory(
        src=os.path.join(repo_root, "include", "opx"),
        dst=os.path.join(library_path, "opx"),
        dry_run=dry_run,
    )

    # ── Generated files ───────────────────────────────────────────────────────
    sync_directory(
        src=os.path.join(repo_root, "autogen"),
        dst=os.path.join(library_path, "autogen"),
        dry_run=dry_run,
    )

    # ── OpxDevice.cpp ─────────────────────────────────────────────────────────
    sync_file(
        src=os.path.join(repo_root, "src", "embedded", "OpxDevice.cpp"),
        dst=os.path.join(library_path, "OpxDevice.cpp"),
        dry_run=dry_run,
    )

    # ── Opx.h and Opx.cpp ─────────────────────────────────────────────────────
    sync_file(
        src=os.path.join(repo_root, "src", "arduino", "Opx.h"),
        dst=os.path.join(library_path, "Opx.h"),
        dry_run=dry_run,
    )
    sync_file(
        src=os.path.join(repo_root, "src", "arduino", "Opx.cpp"),
        dst=os.path.join(library_path, "Opx.cpp"),
        dry_run=dry_run,
    )

    print(f"\n✓ Sync complete.\n")


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────

def main() -> None:
    # Repo root is the directory containing this script
    repo_root = os.path.dirname(os.path.abspath(__file__))

    detected_path = default_arduino_library_path()

    parser = argparse.ArgumentParser(
        description="Sync OmniPlexus library files to your Arduino libraries folder.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"""
Examples:
  python sync_arduino.py
  python sync_arduino.py --library-path ~/MySketchbook/libraries/Opx/src
  python sync_arduino.py --dry-run

Detected default path for your platform ({platform.system()}):
  {detected_path or 'Could not detect — please use --library-path'}
        """,
    )

    parser.add_argument(
        "--library-path",
        default=detected_path,
        metavar="PATH",
        help=(
            "Path to the Arduino library src/ folder "
            f"(default: {detected_path or 'not detected'})"
        ),
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be synced without writing any files",
    )

    args = parser.parse_args()

    if not args.library_path:
        print(
            "Error: Could not detect Arduino library path for your platform.\n"
            "Please provide it manually with --library-path.\n",
            file=sys.stderr,
        )
        sys.exit(1)

    library_path = os.path.expanduser(args.library_path)

    # Warn if autogen/ doesn't exist yet
    autogen_path = os.path.join(repo_root, "autogen")
    if not os.path.isdir(autogen_path):
        print(
            "Warning: autogen/ folder not found. "
            "Run the generator first before syncing.\n"
            f"  python tools/CommandGenerator/generate.py "
            f"manifests/ autogen/ src/autogen/pc/\n",
            file=sys.stderr,
        )

    sync(repo_root, library_path, dry_run=args.dry_run)


if __name__ == "__main__":
    main()