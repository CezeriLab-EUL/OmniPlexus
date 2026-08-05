#
# builder.py
# Top-level orchestration for one SessionBootstrap run.
#
# NOTE: collects WiFi SSID/password here, once per node, if any WiFi
# transport was added and the target isn't pc — this wasn't in our
# earlier flow mock explicitly, but render.py needs it to generate the
# WiFi.begin() block that the sketch (not the library) owns, per the
# earlier finding that EspWiFiTransport never touches credentials.
#

from __future__ import annotations
import os

from prompts import prompt_text, prompt_confirm
from display import print_header, print_success, print_cpp
from writer import write_file

from .identifiers import node_name_valid
from .target import prompt_target, device_class_for, allowed_categories_for
from .transports import build_transports
from .forwarding import build_forwarding_pairs
from .identity import build_identity
from .render import render_stub
from .config import DEFAULT_SUMMARY_THRESHOLD


def build_stub_config_interactive(
    manifests_folder: str, summary_threshold: int = DEFAULT_SUMMARY_THRESHOLD
) -> dict:
    print_header("OmniPlexus Session Bootstrap")

    node_name = prompt_text(
        "Node name (PascalCase, letters/digits only)",
        validator=node_name_valid,
        error_msg="Must start with an uppercase letter and contain only "
        "letters/digits (no underscores or spaces).",
    )

    target = prompt_target()
    device_class = device_class_for(target)
    allowed_categories = allowed_categories_for(target)

    transports = build_transports(
        device_class, allowed_categories, summary_threshold=summary_threshold
    )

    wifi_credentials = None
    if target != "pc" and any(t["category"] == "wifi" for t in transports):
        # PC doesn't need this — WiFi/network association there is an OS
        # concern, not something the sketch manages. Only embedded targets
        # need the raw Arduino WiFi.begin() block.
        print()
        wifi_credentials = {
            "ssid": prompt_text("WiFi SSID"),
            "password": prompt_text("WiFi password"),
        }

    forwarding_pairs = build_forwarding_pairs(transports, device_class)
    identity = build_identity(manifests_folder, node_name, device_class)

    return {
        "node_name": node_name,
        "target": target,
        "device_class": device_class,
        "transports": transports,
        "wifi_credentials": wifi_credentials,
        "forwarding_pairs": forwarding_pairs,
        "identity": identity,
    }


def review_and_write(config: dict, stubs_folder: str) -> bool:
    print_header("Final stub preview")
    source = render_stub(config)
    print_cpp(source)

    node_name = config["node_name"]
    ext = "cpp" if config["target"] == "pc" else "ino"
    out_path = os.path.join(stubs_folder, node_name, f"{node_name}.{ext}")

    if not prompt_confirm(f"Write this stub to {out_path}?", default=True):
        print("Discarded — nothing written.")
        return False

    write_file(out_path, source)
    print_success(f"\u2713 Wrote: {out_path}")
    _print_next_steps(config, out_path)
    return True


def _print_next_steps(config: dict, out_path: str) -> None:
    print_header("Next steps")

    if config["target"] == "pc":
        print("\n1. Regenerate the library's generated headers (if a manifest was added):")
        print("     cmake --build . --target GenerateCommands")
        print("     (or just rebuild — it's part of ALL)")
        print(f"\n2. Build and run: {out_path}")
        print("   PC targets need a build system — this project's CMakeLists.txt")
        print("   is what sets that up; wire the .cpp into your own build if you're")
        print("   not using it.")
    else:
        print("\n1. Regenerate the library's generated headers (if a manifest was added).")
        print("   Pick whichever you actually use:")
        print("     CMake:     cmake --build . --target GenerateCommands")
        print("                then: cmake --build . --target SyncArduinoLibrary")
        print(
            "     No CMake:  python generate_for_arduino.py   (from the Software/ repo root)"
        )
        print("                — writes straight into your Arduino library folder;")
        print("                  no separate sync needed for a manifest-only change")
        print("                  like this one.")
        print(f"\n2. Open and upload: {out_path}")
        print(
            "\n   First time setting up this library on this machine? Also run "
            "sync_arduino.py once"
        )
        print(
            "   (or the CMake SyncArduinoLibrary target) — that copies the core "
            "library files themselves,"
        )
        print("   not just the generated ones.")

    print()
