#
# Node-name validation — the same PascalCase rule ManifestAuthoring uses
# for device names. A node needing its own identity reuses this exact
# name as its manifest device name too (see identity.py) — so the same
# rule has to apply here for that lookup to make sense.
#

from __future__ import annotations


def node_name_valid(name: str) -> bool:
    return name[0:1].isupper() and name.isalnum()
