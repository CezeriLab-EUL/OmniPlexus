#
# Transport-adding loop for one node. Per-category prompts branch on
# device_class where the underlying API genuinely differs:
#
# - OpxSession::connectSerial(const char *port, uint32_t baudRate) takes
#   a filesystem port path (e.g. "/dev/ttyUSB0"), unlike OpxDevice's
#   templated beginSerial(SerialType&, baud) which takes an object
#   reference (Serial1, a SoftwareSerial instance, etc.).

from __future__ import annotations
from prompts import (
    prompt_text,
    prompt_int,
    prompt_int_with_default,
    prompt_confirm,
    prompt_select,
    confirm_shape,
)
from display import print_running_list
from .config import DEFAULT_SUMMARY_THRESHOLD


def _next_instance(transports: list[dict], category: str) -> int:
    used = {t["instance"] for t in transports if t["category"] == category}
    instance = 0
    while instance in used:
        instance += 1
    return instance


def _build_wifi(device_class: str, instance: int) -> dict:
    mode = prompt_select("Mode", ["server", "client"])
    entry: dict = {"category": "wifi", "instance": instance, "mode": mode}

    if mode == "server":
        entry["port"] = prompt_int("Port", min_val=1, max_val=65535)
    else:
        entry["host"] = prompt_text("Host")
        entry["port"] = prompt_int("Port", min_val=1, max_val=65535)
        entry["max_reconnect_attempts"] = prompt_int_with_default(
            "Max reconnect attempts", default=5, min_val=1
        )
        entry["reconnect_delay_ms"] = prompt_int_with_default(
            "Reconnect delay (ms)", default=2000, min_val=0
        )

    # Both WiFi modes start the ESP32 listen task on OpxDevice — client
    # mode still needs to listen for frames from the server it connects to.
    if device_class == "OpxDevice":
        entry["stack_size"] = prompt_int_with_default(
            "FreeRTOS task stack size (bytes)", default=4096, min_val=512
        )

    return entry


def _build_serial(device_class: str, instance: int) -> dict:
    entry: dict = {"category": "serial", "instance": instance}

    if device_class == "OpxSession":
        entry["port_path"] = prompt_text("Serial port path (e.g. /dev/ttyUSB0)")
        entry["baud"] = prompt_int("Baud rate", min_val=1)
        return entry

    serial_kind = prompt_select("Serial kind", ["hardware", "software"])
    entry["serial_kind"] = serial_kind

    if serial_kind == "hardware":
        entry["serial_object"] = prompt_text("Serial object name (e.g. Serial1)")
    else:
        entry["serial_object"] = prompt_text(
            "Name for this SoftwareSerial instance (e.g. softA)"
        )
        entry["rx_pin"] = prompt_int("RX pin", min_val=0)
        entry["tx_pin"] = prompt_int("TX pin", min_val=0)

    entry["baud"] = prompt_int("Baud rate", min_val=1)
    return entry


def _build_http(device_class: str, instance: int) -> dict:
    mode = prompt_select("Mode", ["server", "client"])
    entry: dict = {"category": "http", "instance": instance, "mode": mode}

    if mode == "server":
        entry["port"] = prompt_int("Port", min_val=1, max_val=65535)
        # Only the server path starts the listen task — client (beginHttp
        # Client/connectHttp) takes no stack_size at all in OpxDevice.cpp.
        if device_class == "OpxDevice":
            entry["stack_size"] = prompt_int_with_default(
                "FreeRTOS task stack size (bytes)", default=4096, min_val=512
            )
    else:
        entry["host"] = prompt_text("Host")
        entry["port"] = prompt_int("Port", min_val=1, max_val=65535)

    return entry


_BUILDERS = {
    "wifi": _build_wifi,
    "serial": _build_serial,
    "http": _build_http,
}


def describe_transport(entry: dict) -> str:
    """Short human-readable label — used both for the running summary and
    (imported directly, not duplicated) by forwarding.py's pickers."""
    cat = entry["category"]
    inst = entry["instance"]
    if cat == "wifi":
        detail = f"{entry['mode']}, port {entry['port']}"
    elif cat == "serial":
        detail = entry.get("port_path") or f"{entry['serial_object']}, {entry['baud']} baud"
    elif cat == "http":
        detail = f"{entry['mode']}, port {entry['port']}"
    else:
        detail = ""
    return f"{cat} #{inst} ({detail})"


def _build_transport(device_class: str, transports: list[dict], allowed_categories: list[str]) -> dict:
    while True:
        category = prompt_select("Category", allowed_categories)
        instance = _next_instance(transports, category)
        entry = _BUILDERS[category](device_class, instance)

        if confirm_shape("Transport", entry):
            return entry
        print("Let's redo this transport.")


def build_transports(
    device_class: str,
    allowed_categories: list[str],
    summary_threshold: int = DEFAULT_SUMMARY_THRESHOLD,
) -> list[dict]:
    transports: list[dict] = []

    print("\n--- Transports (at least one is required) ---")
    while True:
        entry = _build_transport(device_class, transports, allowed_categories)
        transports.append(entry)

        if len(transports) >= summary_threshold:
            print_running_list(
                "Transports so far", [describe_transport(t) for t in transports]
            )
        if not prompt_confirm("Add another transport?", default=False):
            break
        print()

    return transports
