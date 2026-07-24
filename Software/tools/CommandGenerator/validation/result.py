from __future__ import annotations
from dataclasses import dataclass, field


@dataclass
class ValidationResult:
    valid: bool = True
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    def add_error(self, message: str) -> None:
        self.valid = False
        self.errors.append(message)

    def add_warning(self, message: str) -> None:
        self.warnings.append(message)

    def merge(self, other: ValidationResult) -> None:
        """Merge another ValidationResult into this one."""
        if not other.valid:
            self.valid = False
        self.errors.extend(other.errors)
        self.warnings.extend(other.warnings)

    def print_results(self, label: str = "") -> None:
        """Print all warnings and errors, then a summary line."""
        if label:
            print(f"\nValidating: {label}...")

        for warning in self.warnings:
            print(f"  ⚠  WARNING: {warning}")

        for error in self.errors:
            print(f"  ✗  ERROR: {error}")

        print(
            f"\nValidation result: "
            f"{len(self.errors)} error(s), "
            f"{len(self.warnings)} warning(s)"
        )
