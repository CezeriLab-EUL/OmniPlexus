from __future__ import annotations
from dataclasses import dataclass, field

from display import print_warning, print_error, print_line


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
            print_warning(warning)

        for error in self.errors:
            print_error(error)

        summary = (
            f"\nValidation result: "
            f"{len(self.errors)} error(s), "
            f"{len(self.warnings)} warning(s)"
        )
        print_line(summary, "green" if self.valid else "red")
