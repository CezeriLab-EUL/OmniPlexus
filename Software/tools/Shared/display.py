#
# Output-only formatting shared by both tools, built on rich. Nothing in
# here asks the user anything — that's prompts.py's job. Kept separate
# so either half (asking vs. displaying) can be reused independently.
#

from __future__ import annotations
from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.syntax import Syntax

console = Console()


def print_header(text: str) -> None:
    """A boxed section header, e.g. for '=== OmniPlexus Manifest Authoring ==='."""
    console.print(Panel(text, style="bold cyan", expand=False))


def print_warning(text: str) -> None:
    console.print(f"  [yellow]\u26a0  WARNING:[/yellow] {text}")


def print_error(text: str) -> None:
    console.print(f"  [red]\u2717  ERROR:[/red] {text}")


def print_success(text: str) -> None:
    console.print(f"[green]{text}[/green]")


def print_note(text: str) -> None:
    """A low-emphasis advisory note (e.g. naming-convention warnings that
    aren't hard errors)."""
    console.print(f"[yellow]  Note: {text}[/yellow]")


def print_table(title: str, columns: list[str], rows: list[tuple]) -> None:
    """Render a generic table — used for the existing-typeShift listing."""
    table = Table(title=title)
    for col in columns:
        table.add_column(col)
    for row in rows:
        table.add_row(*[str(v) for v in row])
    console.print(table)


def print_running_list(label: str, items: list[str]) -> None:
    """A low-emphasis 'X so far: a, b, c' line."""
    console.print(f"[dim]{label}: {', '.join(items)}[/dim]")


def print_code(text: str, lexer: str = "text") -> None:
    """Syntax-highlighted code preview for the given rich/pygments lexer
    name (e.g. 'yaml', 'cpp')."""
    console.print(Syntax(text, lexer, theme="monokai", background_color="default"))


def print_yaml(yaml_text: str) -> None:
    """Syntax-highlighted YAML preview."""
    print_code(yaml_text, "yaml")


def print_cpp(cpp_text: str) -> None:
    """Syntax-highlighted C++ preview."""
    print_code(cpp_text, "cpp")


def print_line(text: str, style: str = "") -> None:
    """Print a single line with an optional rich style (e.g. 'red', 'bold green').
    Used where a message needs a color but doesn't fit the WARNING/ERROR/
    success/note conventions above."""
    if style:
        console.print(f"[{style}]{text}[/{style}]")
    else:
        console.print(text)
