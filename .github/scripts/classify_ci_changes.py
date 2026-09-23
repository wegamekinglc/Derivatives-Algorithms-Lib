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
EXAMPLE_SOURCE_SUFFIXES = frozenset({".cpp", ".cc", ".h", ".hpp", ".hxx"})


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


def benchmark_needed(paths: Sequence[str]) -> bool:
    """Keep performance gates for changes outside documentation and C++ examples."""
    if not paths:
        return True
    for path_text in paths:
        path = PurePosixPath(path_text)
        example_source = (
            not path.is_absolute()
            and ".." not in path.parts
            and path.parts[:2] == ("dal-cpp", "examples")
            and len(path.parts) > 2
            and path.suffix.lower() in EXAMPLE_SOURCE_SUFFIXES
        )
        if not _is_documentation(path_text) and not example_source:
            return True
    return False


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
        paths = changed_paths(options.base, options.head)
    except subprocess.CalledProcessError as error:
        print(
            f"git diff failed with exit code {error.returncode}; running full CI",
            file=sys.stderr,
        )
        paths = ()
    with options.github_output.open("a", encoding="utf-8") as output:
        output.write(f"docs_only={str(docs_only(paths)).lower()}\n")
        output.write(f"benchmark_needed={str(benchmark_needed(paths)).lower()}\n")


if __name__ == "__main__":
    main()
