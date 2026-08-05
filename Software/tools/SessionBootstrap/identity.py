#
# The "does this node handle its own commands, separate from anything it
# forwards" toggle, typeShift resolution, and callback multi-select.
#
# DESIGN NOTE: this looks up a manifest by matching the node's own name
# against manifests' device: fields (via find_manifest_by_device_name,
# which scans file contents rather than assuming filename == device
# name)


from __future__ import annotations

from prompts import prompt_multi_select
from ManifestAuthoring import build_manifest_interactive, review_and_write
from ManifestAuthoring.type_shift import find_manifest_by_device_name

# Both classes support the same set except onConnectionLost, which is
# OpxDevice-only — confirmed absent from OpxSession.h entirely (PC has
# no heartbeat-timeout/connection-lost tracking at all, not just an
# unregistered callback).
_SHARED_CALLBACKS = [
    "onCommand",
    "onTelemetry",
    "onSetting",
    "onResponse",
    "onDeviceConnected",
    "onDeviceDisconnected",
    "onDiscover",
    "onAnnounce",
    "onHeartbeat",
    "onHeartbeatAck",
    "onAnySettingChanged",
]

CALLBACKS_BY_DEVICE_CLASS = {
    "OpxDevice": _SHARED_CALLBACKS + ["onConnectionLost"],
    "OpxSession": list(_SHARED_CALLBACKS),
}

DEFAULT_CHECKED_CALLBACKS = ["onCommand", "onSetting", "onTelemetry"]


def build_identity(manifests_folder: str, node_name: str, device_class: str) -> dict:
    """Always returns {"device_name": str, "type_shift": int,
    "callbacks": [...]} — every node gets a resolved manifest/typeShift
    (register{DeviceName}() must always be called), even a pure-
    forwarding node with zero callbacks selected. device_name is what
    generate.py's register{DeviceName}() function is actually named
    after — needed by render.py to emit the right include and call,
    separately from node_name (which the sketch/folder itself is named
    after)."""

    existing = find_manifest_by_device_name(manifests_folder, node_name)

    if existing is not None:
        device_name = existing["device"]
        type_shift = existing["typeShift"]
        is_identity_only = existing.get("identityOnly", False)
        print(f"\nFound existing manifest for '{node_name}': typeShift={type_shift}")
    else:
        print(f"\nNo manifest found for '{node_name}' — let's author one now.")
        while True:
            manifest = build_manifest_interactive(manifests_folder)
            if review_and_write(manifest, manifests_folder):
                break
            print("Manifest wasn't written — let's try again.")
        device_name = manifest["device"]
        type_shift = manifest["typeShift"]
        is_identity_only = manifest.get("identityOnly", False)
        if device_name != node_name:
            print(
                f"  Note: manifest device name '{device_name}' differs "
                f"from node name '{node_name}'. This is fine, just flagging it "
                f"— the stub will use typeShift={type_shift} either way."
            )

    # An identityOnly manifest has no commands/telemetry/settings defined
    # at all — pre-checking onCommand/onSetting/onTelemetry would check
    # boxes with nothing to register against. Start empty instead; the
    # user can still manually check any callback they actually want.
    default_checked = [] if is_identity_only else DEFAULT_CHECKED_CALLBACKS

    callbacks = prompt_multi_select(
        "Which callbacks should be stubbed?",
        CALLBACKS_BY_DEVICE_CLASS[device_class],
        default_selected=default_checked,
    )

    return {
        "device_name": device_name,
        "type_shift": type_shift,
        "callbacks": callbacks,
    }
