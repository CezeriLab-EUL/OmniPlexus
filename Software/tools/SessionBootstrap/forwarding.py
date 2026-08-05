#
# Optional forward-pairs loop — only asked once a node has 2+ transports
# (forwardBetween() needs two distinct transport IDs; category doesn't
# matter — two Serial instances on the same AVR node qualify same as a
# WiFi+Serial bridge). The "from" transport is excluded from the "to"
# list, since forwarding a transport to itself is meaningless.
#

from __future__ import annotations
from prompts import prompt_select, prompt_confirm, confirm_shape
from .transports import describe_transport


def build_forwarding_pairs(
    transports: list[dict], device_class: str
) -> list[tuple[dict, dict]]:
    # forwardBetween() only exists on OpxDevice — confirmed absent from
    # OpxSession.h entirely, not just unreachable. Must be skipped outright
    # for pc, not just when transport count is low.
    if device_class != "OpxDevice":
        return []

    if len(transports) < 2:
        return []

    if not prompt_confirm(
        "\nShould this node forward frames between any of them?", default=False
    ):
        return []

    pairs: list[tuple[dict, dict]] = []

    while True:
        # describe_transport() always includes category + unique instance,
        # so the label is guaranteed unique — safe to use as a dict key.
        options = {describe_transport(t): t for t in transports}

        from_choice = prompt_select(
            "Forward frames FROM which transport?", list(options.keys())
        )
        from_t = options[from_choice]

        remaining = {k: v for k, v in options.items() if v is not from_t}
        to_choice = prompt_select(
            "Forward frames TO which transport?", list(remaining.keys())
        )
        to_t = remaining[to_choice]

        if confirm_shape("Forwarding pair", {"from": from_choice, "to": to_choice}):
            pairs.append((from_t, to_t))
        else:
            print("Let's redo this pair.")
            continue

        print()
        if not prompt_confirm("Add another forwarding pair?", default=False):
            break

    return pairs
