#!/usr/bin/env python3
"""Fail-gated, dependency-free checks for DAL's published and agent-facing Markdown."""

from __future__ import annotations

import re
import sys
import tomllib
from collections import Counter
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[2]
COMPONENT_READMES = (
    *ROOT.glob("dal-*/README.md"),
    ROOT / "dal-web/backend/README.md",
)
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
# Agent-facing guides sit outside docs/ but carry the same build/test commands;
# they get the standard checks plus referenced-path existence below.
AGENT_DOCS = tuple(
    sorted(
        {
            ROOT / "AGENTS.md",
            ROOT / "CLAUDE.md",
            ROOT / ".github/copilot-instructions.md",
            ROOT / ".codex/README.md",
        }
    )
)
CODEX_DOCS = tuple(sorted((ROOT / ".codex").rglob("*.md")))
GITHUB_DOCS = tuple(sorted((ROOT / ".github").rglob("*.md")))
ALL_DOCS = tuple(sorted({*DOCS, *AGENT_DOCS, *CODEX_DOCS, *GITHUB_DOCS}))

LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^ {0,3}(#{1,6})\s+(.+?)\s*$")
FENCE_RE = re.compile(r"^\s*(`{3,}|~{3,})")
TABLE_DELIMITER_RE = re.compile(r"^:?-{3,}:?$")
TABLE_TOKEN_RE = re.compile(r"\\.|`+|.", flags=re.DOTALL)
BUILD_OPTION_RE = re.compile(r"(?m)^\s*(--[a-z][a-z-]*)\)\s*")
INLINE_CODE_RE = re.compile(r"`([^`]+)`")
AGENT_PATH_TOKEN_RE = re.compile(r"(?<![\w./~:-])(?:\./)?(?:[\w.+-]+/)+[\w.+-]+")
AGENT_PLACEHOLDER_TAIL_RE = re.compile(r"/[^`\s]*[<>*${}~]")

# Repo-root files agent docs name without a directory prefix.
AGENT_ROOT_FILES = {
    "AGENTS.md",
    "CHANGELOG.md",
    "CLAUDE.md",
    "CMakeLists.txt",
    "CMakePresets.json",
    "CONTRIBUTING.md",
    "LICENSE",
    "README.md",
    "build_linux.sh",
    "build_windows.bat",
}
# Directory prefixes that mark a token as a repo-root-relative path claim.
# build/ is deliberately absent: those paths name generated artifacts.
AGENT_PATH_PREFIXES = (
    ".claude/",
    ".codex/",
    ".github/",
    "cmake/",
    "dal-cpp/",
    "dal-excel/",
    "dal-public/",
    "dal-python/",
    "dal-web/",
    "docs/",
    "tests/",
)
# Referenced paths that exist only at runtime or are intentionally local-only.
AGENT_ALLOWED_MISSING = {
    ".claude/settings.local.json",
    ".claude/worktrees",
    "dal-web/backend/.data",
}

AGENT_ROOT_FILE_RE = re.compile(
    r"(?<![\w./~:-])(?:"
    + "|".join(re.escape(name) for name in sorted(AGENT_ROOT_FILES, key=len, reverse=True))
    + r")(?![\w+-])"
)

STALE_DOCUMENTATION = {
    "bin/dal_cpp_tests": "reference the build-tree test binary; staged bin/ installs go stale",
    "bin/dal_public_tests": "reference the build-tree test binary; staged bin/ installs go stale",
    "--gtest_filter=CurveTest.*": "the CurveTest suite does not exist",
    "dal_stub.py": "the web backend has no runtime stub module",
    "DAL_REQUIRE_NATIVE": "native DAL is the only supported production backend",
}

# LaTeX macros GitHub's math renderer fails to display, mapped to a renderable
# replacement. Governed by the macro allow-list rule in .claude/rules/code-style.md
# (Documentation section): math uses only macros GitHub renders.
FORBIDDEN_MATH_MACROS = {
    r"\operatorname": r"\mathrm",
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


def is_external_link(target: str) -> bool:
    split = urlsplit(target)
    return bool(split.scheme or split.netloc or target.startswith(("mailto:", "javascript:")))


def resolve_link_path(document: Path, path_text: str) -> Path:
    if path_text.startswith("/"):
        return (ROOT / path_text.lstrip("/")).resolve()
    if path_text:
        return (document.parent / path_text).resolve()
    return document.resolve()


def existing_link_path(
    document: Path, line_number: int, target: str, fragment: str, errors: list[str]
) -> Path | None:
    linked_path = resolve_link_path(document, urlsplit(target).path)
    try:
        linked_path.relative_to(ROOT)
    except ValueError:
        errors.append(
            f"{relative(document)}:{line_number}: local link escapes the repository: {target}"
        )
        return None

    if linked_path.is_dir():
        if not fragment:
            return None
        linked_path /= "README.md"
    if linked_path.exists():
        return linked_path
    errors.append(f"{relative(document)}:{line_number}: missing local link target: {target}")
    return None


def check_link(
    document: Path,
    line_number: int,
    raw_target: str,
    anchor_cache: dict[Path, set[str]],
    errors: list[str],
) -> None:
    target = unquote(link_target(raw_target))
    if is_external_link(target):
        return
    fragment = urlsplit(target).fragment
    linked_path = existing_link_path(document, line_number, target, fragment, errors)
    if linked_path is None or not fragment or linked_path.suffix.lower() != ".md":
        return
    if linked_path not in anchor_cache:
        anchor_cache[linked_path] = anchors(linked_path)
    if fragment not in anchor_cache[linked_path]:
        errors.append(
            f"{relative(document)}:{line_number}: missing anchor '#{fragment}' "
            f"in {relative(linked_path)}"
        )


def check_links(documents: tuple[Path, ...], errors: list[str]) -> None:
    anchor_cache: dict[Path, set[str]] = {}
    for document in documents:
        lines = document.read_text(encoding="utf-8").splitlines()
        for line_number, line in without_fenced_code(lines):
            for match in LINK_RE.finditer(line):
                check_link(document, line_number, match.group(1), anchor_cache, errors)


def table_row_content(line: str) -> str:
    stripped = line.strip()
    if stripped.startswith("|"):
        stripped = stripped[1:]
    if stripped.endswith("|") and not stripped.endswith(r"\|"):
        stripped = stripped[:-1]
    return stripped


def split_table_row(line: str) -> list[str]:
    stripped = table_row_content(line)
    cells: list[str] = []
    current: list[str] = []
    code_delimiter = 0
    for match in TABLE_TOKEN_RE.finditer(stripped):
        token = match.group(0)
        if token.startswith("\\"):
            current.append(token)
        elif token.startswith("`"):
            run = len(token)
            current.append(token)
            code_delimiter = 0 if code_delimiter == run else run
        elif token == "|" and code_delimiter == 0:
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(token)
    cells.append("".join(current).strip())
    return cells


def table_header_cells(visible: dict[int, str], line_number: int) -> tuple[list[str], list[str]] | None:
    header = visible.get(line_number)
    delimiter = visible.get(line_number + 1)
    if header is None or delimiter is None:
        return None
    if "|" not in header or "|" not in delimiter:
        return None
    header_cells = split_table_row(header)
    delimiter_cells = split_table_row(delimiter)
    if not delimiter_cells:
        return None
    if not all(TABLE_DELIMITER_RE.fullmatch(cell) for cell in delimiter_cells):
        return None
    return header_cells, delimiter_cells


def is_table_body_row(row: str | None) -> bool:
    return row is not None and bool(row.strip()) and "|" in row


def check_table_body(
    document: Path,
    raw_line_count: int,
    visible: dict[int, str],
    row_number: int,
    expected: int,
    errors: list[str],
) -> int:
    while row_number <= raw_line_count:
        row = visible.get(row_number)
        if not is_table_body_row(row):
            break
        actual = len(split_table_row(row or ""))
        if actual != expected:
            errors.append(
                f"{relative(document)}:{row_number}: table row has {actual} cells; expected {expected}"
            )
        row_number += 1
    return row_number


def check_tables(documents: tuple[Path, ...], errors: list[str]) -> None:
    for document in documents:
        raw_lines = document.read_text(encoding="utf-8").splitlines()
        visible = dict(without_fenced_code(raw_lines))
        line_number = 1
        while line_number < len(raw_lines):
            cells = table_header_cells(visible, line_number)
            if cells is None:
                line_number += 1
                continue
            header_cells, delimiter_cells = cells
            if len(header_cells) != len(delimiter_cells):
                errors.append(
                    f"{relative(document)}:{line_number + 1}: table delimiter has "
                    f"{len(delimiter_cells)} cells; header has {len(header_cells)}"
                )
            row_number = check_table_body(
                document, len(raw_lines), visible, line_number + 2, len(header_cells), errors
            )
            line_number = max(line_number + 1, row_number)


def check_whitespace(documents: tuple[Path, ...], errors: list[str]) -> None:
    for document in documents:
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


def check_stale_commands(documents: tuple[Path, ...], errors: list[str]) -> None:
    for document in documents:
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


def check_math_macros(documents: tuple[Path, ...], errors: list[str]) -> None:
    # Only scan visible (non-fenced) lines so a macro shown literally inside a
    # code fence is not flagged. Math lives in `$...$` / `$$...$$` on these lines.
    for document in documents:
        lines = document.read_text(encoding="utf-8").splitlines()
        for line_number, line in without_fenced_code(lines):
            visible = INLINE_CODE_RE.sub("", line)
            for bad, good in FORBIDDEN_MATH_MACROS.items():
                if bad in visible:
                    errors.append(
                        f"{relative(document)}:{line_number}: GitHub does not render "
                        f"'{bad}' in math; use '{good}' instead"
                    )


def code_regions(lines: list[str]) -> list[tuple[int, str]]:
    # Path claims are checked only where conventions put them: fenced code and
    # inline code spans. Prose (e.g. "docs/changelog") is not parsed.
    regions: list[tuple[int, str]] = []
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
        if closing_marker is not None:
            regions.append((line_number, line))
        else:
            for span in INLINE_CODE_RE.finditer(line):
                regions.append((line_number, span.group(1)))
    return regions


def agent_path_claim(token: str, tail: str) -> str | None:
    if token.startswith("./"):
        token = token[2:]
    token = token.rstrip(".,:;")
    if any(marker in token for marker in "<>*${}~"):
        return None
    if tail and (tail[0] in "<>*${}~" or AGENT_PLACEHOLDER_TAIL_RE.match(tail)):
        return None
    if not token.startswith(AGENT_PATH_PREFIXES):
        return None
    return token


def agent_referenced_paths(text: str) -> list[tuple[int, str]]:
    referenced: list[tuple[int, str]] = []
    for line_number, segment in code_regions(text.splitlines()):
        found: list[tuple[int, str]] = [(m.start(), m.group(0)) for m in AGENT_ROOT_FILE_RE.finditer(segment)]
        for match in AGENT_PATH_TOKEN_RE.finditer(segment):
            token = agent_path_claim(match.group(0), segment[match.end() :])
            if token is not None:
                found.append((match.start(), token))
        for _, token in sorted(found):
            referenced.append((line_number, token))
    return referenced


def check_agent_paths(documents: tuple[Path, ...], root: Path, errors: list[str]) -> None:
    for document in documents:
        text = document.read_text(encoding="utf-8")
        for line_number, token in agent_referenced_paths(text):
            if token in AGENT_ALLOWED_MISSING or (root / token).exists():
                continue
            errors.append(
                f"{document.relative_to(root).as_posix()}:{line_number}: "
                f"referenced repository path does not exist: {token}"
            )


def main() -> int:
    errors: list[str] = []
    check_links(ALL_DOCS, errors)
    check_tables(ALL_DOCS, errors)
    check_whitespace(ALL_DOCS, errors)
    check_metadata(errors)
    check_stale_commands(ALL_DOCS, errors)
    check_math_macros(ALL_DOCS, errors)
    check_agent_paths(AGENT_DOCS, ROOT, errors)

    if errors:
        print("Documentation checks failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(f"Documentation checks passed for {len(ALL_DOCS)} Markdown files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
