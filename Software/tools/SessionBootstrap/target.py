#
# Target selection and per-target capabilities.
#
# - esp32: beginWiFi/beginHttpServer/beginHttpClient/connectWiFi are all
#   wrapped in `#if OPX_TARGET_ESP32` in OpxDevice.h. avr doesn't get
#   those methods at all — not disabled, they simply don't exist in the
#   compiled class for that target.
# - pc: OpxSession compiles connectWiFi/connectSerial/connectHttp/
#   beginWiFi/beginHttpServer unconditionally (guarded only by
#   `#ifndef OPX_TARGET_EMBEDDED` at the class level).
#

from __future__ import annotations
from prompts import prompt_select

TARGETS = ["esp32", "avr", "pc"]

TARGET_DEVICE_CLASS = {
    "esp32": "OpxDevice",
    "avr": "OpxDevice",
    "pc": "OpxSession",
}

TARGET_TRANSPORT_CATEGORIES = {
    "esp32": ["wifi", "serial", "http"],
    "avr": ["serial"],
    "pc": ["wifi", "serial", "http"],
}


def prompt_target() -> str:
    return prompt_select("Target", TARGETS)


def device_class_for(target: str) -> str:
    return TARGET_DEVICE_CLASS[target]


def allowed_categories_for(target: str) -> list[str]:
    return TARGET_TRANSPORT_CATEGORIES[target]
