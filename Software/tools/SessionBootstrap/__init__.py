import os
import sys

# Shared/ (utils, writer, validation, prompts, display) — plain top-level
# imports like `from prompts import ...` need Shared/ itself on sys.path.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "Shared"))

# tools/ itself — needed so `from ManifestAuthoring import ...` resolves,
# since identity.py delegates manifest authoring there directly rather
# than reimplementing it
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from .builder import build_stub_config_interactive, review_and_write

__all__ = [
    "build_stub_config_interactive",
    "review_and_write",
]
