#!/usr/bin/env python3
"""Fail-gated, dependency-free checks for DAL's published Markdown."""

from __future__ import annotations

import re
import sys
import tomllib
from collections import Counter
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[2]
COMPONENT_READMES = tuple(ROOT.glob("dal-*/README.md"))
DOCS = tuple(
    sorted(
        {
            ROOT / "README.md",
            ROOT / "CONTRIBUTING.md",
            ROOT / "CHANGELOG.md",
            *COMPONENT_READMES,
            *(ROOT / "docs").rglob("*.md"),
        }
    )
)

LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^ {0,3}(#{1,6})\s+(.+?)\s*$")
FENCE_RE = re.compile(r"^\s*(`{3,}|~{3,})")
TABLE_DELIMITER_RE = re.compile(r"^:?-{3,}:?$")
BUILD_OPTION_RE = re.compile(r"(?m)^\s*(--[a-z][a-z-]*)\)\s*")

STALE_DOCUMENTATION = {
    "bin/dal_public_tests": "dal_public_tests is a build-tree target, not an installed binary",
    "--gtest_filter=CurveTest.*": "the CurveTest suite does not exist",
    "dal_stub.py": "the web backend has no runtime stub module",
    "DAL_REQUIRE_NATIVE": "native DAL is the only supported production backend",
}


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def without_fenced_code(lines: list[str]) -> list[tuple[int, str]]:
    visible: list[tuple[int, str]] = []
    closing_marker: str | None = None
    for line_number, line in enumerate(lines, start=1):
        marker = FENCE_RE.match(line)
        if marker:
            token = marker.group(1)
            if closing_marker is None:
                closing_marker = token[0]
            elif token[0] == closing_marker:
                closing_marker = None
            continue
        if closing_marker is None:
            visible.append((line_number, line))
    return visible


def github_slug(text: str) -> str:
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"!?\[([^\]]+)\]\([^)]*\)", r"\1", text)
    text = text.replace("`", "").replace("*", "").replace("_", "_")
    text = re.sub(r"[^\w\- ]", "", text.lower(), flags=re.UNICODE)
    return re.sub(r"\s", "-", text.strip())


def anchors(path: Path) -> set[str]:
    result: set[str] = set()
    counts: Counter[str] = Counter()
    lines = path.read_text(encoding="utf-8").splitlines()
    for _, line in without_fenced_code(lines):
        match = HEADING_RE.match(line)
        if not match:
            continue
        heading = re.sub(r"\s+#+\s*$", "", match.group(2))
        base = github_slug(heading)
        count = counts[base]
        counts[base] += 1
        result.add(base if count == 0 else f"{base}-{count}")
    return result


def link_target(raw_target: str) -> str:
    target = raw_target.strip()
    if target.startswith("<") and ">" in target:
        return target[1 : target.index(">")]
    # Markdown titles follow the URL after whitespace. Published DAL paths do
    # not contain unescaped spaces, so the first token is the link destination.
    return target.split(maxsplit=1)[0]


def check_links(errors: list[str]) -> None:
    anchor_cache: dict[Path, set[str]] = {}
    for document in DOCS:
        lines = document.read_text(encoding="utf-8").splitlines()
        for line_number, line in without_fenced_code(lines):
            for match in LINK_RE.finditer(line):
                target = unquote(link_target(match.group(1)))
                split = urlsplit(target)
                if split.scheme or split.netloc or target.startswith(("mailto:", "javascript:")):
                    continue

                path_text = split.path
                if path_text.startswith("/"):
                    linked_path = ROOT / path_text.lstrip("/")
                elif path_text:
                    linked_path = document.parent / path_text
                else:
                    linked_path = document
                linked_path = linked_path.resolve()

                try:
                    linked_path.relative_to(ROOT)
                except ValueError:
                    errors.append(
                        f"{relative(document)}:{line_number}: local link escapes the repository: {target}"
                    )
                    continue

                if linked_path.is_dir():
                    if not split.fragment:
                        continue
                    linked_path /= "README.md"
                if not linked_path.exists():
                    errors.append(
                        f"{relative(document)}:{line_number}: missing local link target: {target}"
                    )
                    continue

                fragment = split.fragment
                if fragment and linked_path.suffix.lower() == ".md":
                    if linked_path not in anchor_cache:
                        anchor_cache[linked_path] = anchors(linked_path)
                    if fragment not in anchor_cache[linked_path]:
                        errors.append(
                            f"{relative(document)}:{line_number}: missing anchor '#{fragment}' "
                            f"in {relative(linked_path)}"
                        )


def split_table_row(line: str) -> list[str]:
    stripped = line.strip()
    if stripped.startswith("|"):
        stripped = stripped[1:]
    if stripped.endswith("|") and not stripped.endswith(r"\|"):
        stripped = stripped[:-1]

    cells: list[str] = []
    current: list[str] = []
    escaped = False
    code_delimiter = 0
    index = 0
    while index < len(stripped):
        char = stripped[index]
        if escaped:
            current.append(char)
            escaped = False
        elif char == "\\":
            current.append(char)
            escaped = True
        elif char == "`":
            run = 1
            while index + run < len(stripped) and stripped[index + run] == "`":
                run += 1
            current.extend("`" * run)
            code_delimiter = 0 if code_delimiter == run else run
            index += run - 1
        elif char == "|" and code_delimiter == 0:
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(char)
        index += 1
    cells.append("".join(current).strip())
    return cells


def check_tables(errors: list[str]) -> None:
    for document in DOCS:
        raw_lines = document.read_text(encoding="utf-8").splitlines()
        visible = dict(without_fenced_code(raw_lines))
        line_number = 1
        while line_number < len(raw_lines):
            header = visible.get(line_number)
            delimiter = visible.get(line_number + 1)
            if header is None or delimiter is None or "|" not in header or "|" not in delimiter:
                line_number += 1
                continue
            header_cells = split_table_row(header)
            delimiter_cells = split_table_row(delimiter)
            if not delimiter_cells or not all(TABLE_DELIMITER_RE.fullmatch(cell) for cell in delimiter_cells):
                line_number += 1
                continue
            if len(header_cells) != len(delimiter_cells):
                errors.append(
                    f"{relative(document)}:{line_number + 1}: table delimiter has "
                    f"{len(delimiter_cells)} cells; header has {len(header_cells)}"
                )
            expected = len(header_cells)
            row_number = line_number + 2
            while row_number <= len(raw_lines):
                row = visible.get(row_number)
                if row is None or not row.strip() or "|" not in row:
                    break
                actual = len(split_table_row(row))
                if actual != expected:
                    errors.append(
                        f"{relative(document)}:{row_number}: table row has {actual} cells; "
                        f"expected {expected}"
                    )
                row_number += 1
            line_number = max(line_number + 1, row_number)


def check_whitespace(errors: list[str]) -> None:
    for document in DOCS:
        for line_number, line in enumerate(
            document.read_text(encoding="utf-8").splitlines(keepends=True), start=1
        ):
            content = line.rstrip("\r\n")
            if content.endswith((" ", "\t")):
                errors.append(f"{relative(document)}:{line_number}: trailing whitespace")


def check_metadata(errors: list[str]) -> None:
    license_text = (ROOT / "LICENSE").read_text(encoding="utf-8")
    if not license_text.startswith("MIT License"):
        errors.append("LICENSE: expected the repository license to be MIT")

    with (ROOT / "dal-python/pyproject.toml").open("rb") as stream:
        metadata = tomllib.load(stream)
    python_license = metadata.get("project", {}).get("license")
    if isinstance(python_license, dict):
        python_license = python_license.get("text")
    if python_license != "MIT":
        errors.append(
            "dal-python/pyproject.toml: project.license must match the repository MIT license"
        )

    for document in DOCS:
        text = document.read_text(encoding="utf-8")
        if re.search(r"BSD[ -]?3(?:-Clause)?", text, flags=re.IGNORECASE):
            errors.append(f"{relative(document)}: stale BSD-3-Clause license metadata")


def check_stale_commands(errors: list[str]) -> None:
    for document in DOCS:
        text = document.read_text(encoding="utf-8")
        for stale, explanation in STALE_DOCUMENTATION.items():
            for match in re.finditer(re.escape(stale), text):
                line_number = text.count("\n", 0, match.start()) + 1
                errors.append(f"{relative(document)}:{line_number}: {explanation}: {stale}")

    build_script = (ROOT / "build_linux.sh").read_text(encoding="utf-8")
    implemented = set(BUILD_OPTION_RE.findall(build_script))
    installation = (ROOT / "docs/installation.md").read_text(encoding="utf-8")
    try:
        option_section = installation.split("### Script options", maxsplit=1)[1]
        option_section = re.split(r"^#{2,3}\s", option_section, maxsplit=1, flags=re.MULTILINE)[0]
    except IndexError:
        errors.append("docs/installation.md: missing canonical 'Script options' section")
        return
    documented = set(re.findall(r"\|\s*`(--[a-z][a-z-]*)`\s*\|", option_section))
    if documented != implemented:
        errors.append(
            "docs/installation.md: build_linux.sh option table drift: "
            f"implemented={sorted(implemented)}, documented={sorted(documented)}"
        )


def main() -> int:
    errors: list[str] = []
    check_links(errors)
    check_tables(errors)
    check_whitespace(errors)
    check_metadata(errors)
    check_stale_commands(errors)

    if errors:
        print("Documentation checks failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(f"Documentation checks passed for {len(DOCS)} Markdown files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
