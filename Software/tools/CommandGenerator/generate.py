#!/usr/bin/env python3
#
# Usage:
#   python generate.py <manifests_folder> <header_output_dir> <source_output_dir>
#   python generate.py --validate-only <manifests_folder>
#
# Options:
#   --validate-only       Run validation only, do not generate any files
#   --max-params N        Maximum number of parameters per command (default: 3)
#

from __future__ import annotations
import argparse
import os
import sys

import yaml

from validation import validate, validate_all_cross_device
from writer import write_files
from generators import (
    command_types,
    command_packer,
    command_registry,
    generated_config,
    device_manifest,
    telemetry_source_ids,
    setting_ids,
    controller,
    register_all,
    register,
    opx_devices,
)

TOOL_NAME = "OmniPlexus CommandGenerator"


# ─────────────────────────────────────────────────────────────────────────────
# Manifest discovery
# ─────────────────────────────────────────────────────────────────────────────


def discover_manifests(folder: str) -> list[str]:
    """Return sorted list of .yaml files in the given folder."""
    if not os.path.isdir(folder):
        print(f"Error: manifests folder not found: {folder}", file=sys.stderr)
        sys.exit(1)

    files = sorted(
        os.path.join(folder, f)
        for f in os.listdir(folder)
        if f.endswith(".yaml") or f.endswith(".yml")
    )

    if not files:
        print(f"Error: no YAML manifest files found in: {folder}", file=sys.stderr)
        sys.exit(1)

    return files


# ─────────────────────────────────────────────────────────────────────────────
# Loading and validation
# ─────────────────────────────────────────────────────────────────────────────


def load_and_validate(
    manifest_paths: list[str],
    max_params: int,
) -> list[dict]:
    """Load all manifests, validate each one, return list of valid data dicts.
    Exits with code 1 if any manifest fails validation."""
    all_data: list[dict] = []
    any_error = False

    for path in manifest_paths:
        print(f"\nReading: {path}")
        try:
            with open(path, encoding="utf-8") as f:
                data = yaml.safe_load(f)
        except yaml.YAMLError as e:
            print(f"Error: Failed to parse YAML: {path}\n  {e}", file=sys.stderr)
            any_error = True
            continue
        except OSError as e:
            print(f"Error: Could not open file: {path}\n  {e}", file=sys.stderr)
            any_error = True
            continue

        print("YAML parsed successfully.")

        result = validate(data, max_params=max_params)
        result.print_results(label=path)

        if not result.valid:
            any_error = True
        else:
            all_data.append(data)

    if any_error:
        print(
            "\nGeneration aborted due to errors. Fix the issues above and try again.\n",
            file=sys.stderr,
        )
        sys.exit(1)

    # Cross-device validation
    print("\nRunning cross-device validation...")
    cross_result = validate_all_cross_device(all_data)
    cross_result.print_results()

    if not cross_result.valid:
        print("\nGeneration aborted due to cross-device errors.\n", file=sys.stderr)
        sys.exit(1)

    return all_data


# ─────────────────────────────────────────────────────────────────────────────
# File generation
# ─────────────────────────────────────────────────────────────────────────────


def generate_files(
    all_data: list[dict],
    header_output_dir: str,
    source_output_dir: str,
) -> None:
    """Generate all output files from validated device data."""

    def path(*parts: str) -> str:
        return os.path.join(*parts)

    shared_dir = path(header_output_dir, "shared")
    embedded_dir = path(header_output_dir, "embedded")
    pc_dir = path(header_output_dir, "pc")

    has_telemetry = any(d.get("telemetry") for d in all_data)
    has_settings = any(d.get("settings") for d in all_data)

    # ── Shared files ──────────────────────────────────────────────────────────
    files: dict[str, str] = {
        path(shared_dir, "CommandTypes.h"): command_types.generate(all_data),
        path(shared_dir, "CommandPacker.h"): command_packer.generate(all_data),
        path(shared_dir, "GeneratedConfig.h"): generated_config.generate(all_data),
        path(shared_dir, "OpxDevices.h"): opx_devices.generate(all_data),
        path(pc_dir, "DeviceManifest.h"): device_manifest.generate(all_data),
        path(source_output_dir, "CommandRegistry.cpp"): command_registry.generate(
            all_data
        ),
    }

    if has_telemetry:
        files[path(shared_dir, "TelemetrySourceIDs.h")] = telemetry_source_ids.generate(
            all_data
        )

    if has_settings:
        files[path(shared_dir, "SettingIDs.h")] = setting_ids.generate(all_data)

    # ── Per-device files ──────────────────────────────────────────────────────
    for data in all_data:
        device_name = data["device"]
        target = data["target"]

        shared_device_dir = path(shared_dir, "devices", device_name)

        files[path(shared_device_dir, f"{device_name}Controller.h")] = (
            controller.generate(data)
        )

        if target == "pc":
            target_device_dir = path(pc_dir, "devices", device_name)
        else:
            target_device_dir = path(embedded_dir, "devices", device_name)

        if not data.get("identityOnly", False):
            files[path(target_device_dir, f"{device_name}RegisterAll.h")] = (
                register_all.generate(data)
            )

        files[path(target_device_dir, f"{device_name}Register.h")] = register.generate(
            data
        )

    print("\nGenerating files...")
    write_files(files)


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────


def main() -> None:
    print(f"{TOOL_NAME}")
    print("-" * 40)

    parser = argparse.ArgumentParser(
        description=f"{TOOL_NAME} — generates C++ files from YAML device manifests",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python generate.py manifests/ autogen/ src/autogen/pc/
  python generate.py --validate-only manifests/
  python generate.py manifests/ autogen/ src/autogen/pc/ --max-params 5
        """,
    )

    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="Run validation only, do not generate any files",
    )
    parser.add_argument(
        "--max-params",
        type=int,
        default=3,
        metavar="N",
        help="Maximum number of parameters per command (default: 3)",
    )
    parser.add_argument(
        "manifests_folder",
        help="Folder containing YAML device manifest files",
    )
    parser.add_argument(
        "header_output_dir",
        nargs="?",
        help="Output directory for generated headers (not required with --validate-only)",
    )
    parser.add_argument(
        "source_output_dir",
        nargs="?",
        help="Output directory for generated source files (not required with --validate-only)",
    )

    args = parser.parse_args()

    # Validate arg combinations
    if not args.validate_only:
        if not args.header_output_dir or not args.source_output_dir:
            parser.error(
                "header_output_dir and source_output_dir are required "
                "unless --validate-only is specified"
            )

    # Discover and load manifests
    manifest_paths = discover_manifests(args.manifests_folder)
    all_data = load_and_validate(manifest_paths, max_params=args.max_params)

    total_commands = sum(len(d.get("commands", [])) for d in all_data)

    if args.validate_only:
        print(
            f"\n✓ Validation passed! {total_commands} command(s) validated "
            f"across {len(all_data)} device(s)."
        )
        print("(--validate-only mode: no files generated)\n")
        return

    # Generate
    try:
        generate_files(all_data, args.header_output_dir, args.source_output_dir)
    except RuntimeError as e:
        print(f"\nError during generation: {e}", file=sys.stderr)
        sys.exit(1)

    print(
        f"\n✓ Done! {total_commands} command(s) processed "
        f"across {len(all_data)} device(s).\n"
    )


if __name__ == "__main__":
    main()
