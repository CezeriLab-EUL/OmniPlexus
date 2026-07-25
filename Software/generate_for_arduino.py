#!/usr/bin/env python3
#
# generate_for_arduino.py
# OmniPlexus — Generate device files directly into the Arduino library folder
#
# Automatically detects your Arduino library path and runs the generator
# with the correct output directories so generated files land exactly
# where the Arduino build system expects them.
#
# Usage:
#   python generate_for_arduino.py
#   python generate_for_arduino.py --manifests-folder path/to/manifests
#   python generate_for_arduino.py --library-path ~/MySketchbook/libraries/Opx/src
#   python generate_for_arduino.py --dry-run
#   python generate_for_arduino.py --validate-only
#   python generate_for_arduino.py --max-params 5
#

from __future__ import annotations
import argparse
import os
import platform
import subprocess
import sys

LIBRARY_NAME = "Opx"

# ─────────────────────────────────────────────────────────────────────────────
# Platform detection (shared logic with sync_arduino.py)
# ─────────────────────────────────────────────────────────────────────────────


def default_arduino_library_path() -> str | None:
    """Return the default Arduino libraries src/ path for the current platform."""
    system = platform.system()

    if system == "Linux":
        return os.path.expanduser(f"~/Arduino/libraries/{LIBRARY_NAME}/src")
    elif system == "Darwin":  # macOS
        return os.path.expanduser(f"~/Documents/Arduino/libraries/{LIBRARY_NAME}/src")
    elif system == "Windows":
        documents = os.path.join(os.path.expanduser("~"), "Documents")
        return os.path.join(documents, "Arduino", "libraries", LIBRARY_NAME, "src")
    else:
        return None


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────


def main() -> None:
    repo_root = os.path.dirname(os.path.abspath(__file__))
    detected_path = default_arduino_library_path()
    generator_path = os.path.join(repo_root, "tools", "CommandGenerator", "generate.py")

    parser = argparse.ArgumentParser(
        description=(
            "Generate OmniPlexus device files directly into your Arduino library folder.\n"
            "Automatically detects the correct output paths — no manual path entry needed."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"""
Examples:
  python generate_for_arduino.py
  python generate_for_arduino.py --manifests-folder ~/myrobot/manifests
  python generate_for_arduino.py --library-path ~/MySketchbook/libraries/Opx/src
  python generate_for_arduino.py --dry-run
  python generate_for_arduino.py --validate-only

Detected default path for your platform ({platform.system()}):
  {detected_path or 'Could not detect — please use --library-path'}
        """,
    )

    parser.add_argument(
        "--manifests-folder",
        default=os.path.join(repo_root, "manifests"),
        metavar="PATH",
        help="Folder containing your YAML device manifest files (default: manifests/)",
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
        help="Show what would be generated without writing any files",
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="Validate manifests only, do not generate any files",
    )
    parser.add_argument(
        "--max-params",
        type=int,
        default=3,
        metavar="N",
        help=(
            "Maximum number of parameters per command (default: 3). "
            "Strongly recommended to keep at 3 for embedded targets."
        ),
    )

    args = parser.parse_args()

    # ── Validate inputs ───────────────────────────────────────────────────────
    if not args.validate_only and not args.library_path:
        print(
            "Error: Could not detect Arduino library path for your platform.\n"
            "Please provide it manually with --library-path.\n",
            file=sys.stderr,
        )
        sys.exit(1)

    manifests_folder = os.path.expanduser(args.manifests_folder)
    if not os.path.isdir(manifests_folder):
        print(
            f"Error: Manifests folder not found: {manifests_folder}\n"
            f"Create the folder and add your YAML device manifest files.\n",
            file=sys.stderr,
        )
        sys.exit(1)

    if not os.path.isfile(generator_path):
        print(
            f"Error: Generator script not found: {generator_path}\n"
            f"Make sure you are running this script from the OmniPlexus repo root.\n",
            file=sys.stderr,
        )
        sys.exit(1)

    # ── Build generator arguments ─────────────────────────────────────────────
    if args.validate_only:
        cmd = [
            sys.executable,
            generator_path,
            "--validate-only",
            manifests_folder,
        ]
    else:
        library_path = os.path.expanduser(args.library_path)
        header_output = os.path.join(library_path, "autogen")
        source_output = os.path.join(library_path, "autogen", "pc")

        cmd = [
            sys.executable,
            generator_path,
            manifests_folder,
            header_output,
            source_output,
            "--max-params",
            str(args.max_params),
        ]

        print(f"\nOmniPlexus — Generate for Arduino")
        print(f"-" * 40)
        print(f"  Manifests:   {manifests_folder}")
        print(f"  Library:     {library_path}")
        print(f"  Headers →    {header_output}")
        print(f"  Sources →    {source_output}")

        if args.dry_run:
            print(f"\n[dry-run] Would run:\n  {' '.join(cmd)}\n")
            return

        print()

    # ── Run generator ─────────────────────────────────────────────────────────
    generator_dir = os.path.dirname(generator_path)
    result = subprocess.run(cmd, cwd=generator_dir)
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
