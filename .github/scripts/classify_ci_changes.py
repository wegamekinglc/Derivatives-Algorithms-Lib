"""Classify a Git diff for the lightweight documentation CI path."""

from __future__ import annotations

import argparse
import subprocess  # nosec B404 -- fixed, repository-owned git command only
import sys
from collections.abc import Sequence
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[2]
DOCUMENTATION_SUFFIXES = frozenset({".md", ".mdx", ".rst"})
DOCUMENTATION_ROOT_FILES = frozenset({"LICENSE", "NOTICE"})


def _is_documentation(path_text: str) -> bool:
    path = PurePosixPath(path_text)
    if path.is_absolute() or ".." in path.parts:
        return False
    return (
        path.parts[:1] == ("docs",)
        or path.suffix.lower() in DOCUMENTATION_SUFFIXES
        or path.as_posix() in DOCUMENTATION_ROOT_FILES
    )


def docs_only(paths: Sequence[str]) -> bool:
    """Return whether every changed path is documentation-only."""
    return bool(paths) and all(_is_documentation(path) for path in paths)


def changed_paths(base: str, head: str) -> tuple[str, ...]:
    """Return NUL-safe changed paths between two repository revisions."""
    if base and set(base) == {"0"}:
        return ()
    completed = subprocess.run(  # nosec B603  # nosemgrep
        (
            "git",
            "diff",
            "--no-renames",
            "--name-only",
            "-z",
            base,
            head,
            "--",
        ),
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return tuple(
        path.decode("utf-8", errors="surrogateescape")
        for path in completed.stdout.split(b"\0")
        if path
    )


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", required=True)
    parser.add_argument("--github-output", type=Path, required=True)
    return parser


def main(arguments: Sequence[str] | None = None) -> None:
    options = _parser().parse_args(arguments)
    try:
        classification = docs_only(changed_paths(options.base, options.head))
    except subprocess.CalledProcessError as error:
        print(
            f"git diff failed with exit code {error.returncode}; running full CI",
            file=sys.stderr,
        )
        classification = False
    with options.github_output.open("a", encoding="utf-8") as output:
        output.write(f"docs_only={str(classification).lower()}\n")


if __name__ == "__main__":
    main()
