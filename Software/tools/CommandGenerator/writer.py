from __future__ import annotations
import os


def ensure_directory(path: str) -> None:
    """Create a directory and all intermediate parents if they don't exist."""
    os.makedirs(path, exist_ok=True)


def write_file(path: str, content: str) -> None:
    """Write content to a file, creating parent directories as needed.
    Raises RuntimeError if the file cannot be written."""
    ensure_directory(os.path.dirname(path))
    try:
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
    except OSError as e:
        raise RuntimeError(f"Could not write file: {path}\n  {e}") from e


def write_files(files: dict[str, str]) -> None:
    """Write multiple files at once.

    Args:
        files: A dict mapping output path -> file content.
               All parent directories are created automatically.
    """
    for path, content in files.items():
        write_file(path, content)
        print(f"  ✓ Generated: {path}")
