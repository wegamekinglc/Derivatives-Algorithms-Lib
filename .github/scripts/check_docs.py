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
COMPONENT_READMES = (*ROOT.glob("dal-*/README.md"),)
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
    "docs/",
    "tests/",
)
# Referenced paths that exist only at runtime or are intentionally local-only.
AGENT_ALLOWED_MISSING = {
    ".claude/settings.local.json",
    ".claude/worktrees",
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


def check_build_script_options(build_script: str, installation: str, errors: list[str]) -> None:
    implemented = set(BUILD_OPTION_RE.findall(build_script))
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


def check_stale_commands(documents: tuple[Path, ...], errors: list[str]) -> None:
    for document in documents:
        text = document.read_text(encoding="utf-8")
        for stale, explanation in STALE_DOCUMENTATION.items():
            for match in re.finditer(re.escape(stale), text):
                line_number = text.count("\n", 0, match.start()) + 1
                errors.append(f"{relative(document)}:{line_number}: {explanation}: {stale}")

    check_build_script_options(
        (ROOT / "build_linux.sh").read_text(encoding="utf-8"),
        (ROOT / "docs/installation.md").read_text(encoding="utf-8"),
        errors,
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


def check_current_state_doc_locations(errors: list[str], root: Path = ROOT) -> None:
    historical_root = root / "docs/superpowers"
    for document in sorted(historical_root.rglob("*.md")) if historical_root.exists() else ():
        errors.append(
            f"{document.relative_to(root).as_posix()}: completed implementation plans and "
            "design history do not belong under current-state docs/"
        )


def check_ci_compiler_inventory(errors: list[str]) -> None:
    workflow = (ROOT / ".github/workflows/cmake-linux.yml").read_text(encoding="utf-8")
    match = re.search(r"^\s*compiler:\s*\[([^]]+)\]", workflow, flags=re.MULTILINE)
    if match is not None:
        compilers = {value.strip() for value in match.group(1).split(",")}
    else:
        # The matrix is produced by the define-matrix job; the compiler
        # inventory lives in its generator table as "compiler": "<name>".
        compilers = set(re.findall(r'"compiler":\s*"([^"]+)"', workflow))
    if not compilers:
        errors.append(".github/workflows/cmake-linux.yml: compiler matrix was not found")
        return
    copilot = (ROOT / ".github/copilot-instructions.md").read_text(encoding="utf-8")
    missing = sorted(compiler for compiler in compilers if compiler not in copilot)
    if missing:
        errors.append(
            ".github/copilot-instructions.md: CI compiler inventory omits "
            f"{missing}"
        )


def check_windows_python_helpers(errors: list[str]) -> None:
    for relative_path in ("dal-python/run_tests.ps1", "dal-python/build_wheel.ps1"):
        text = (ROOT / relative_path).read_text(encoding="utf-8")
        normalized = text.replace("\\", "/")
        if "build_win.ps1" in text:
            errors.append(f"{relative_path}: references nonexistent build_win.ps1")
        if "build/stage/Release-windows" not in normalized:
            errors.append(f"{relative_path}: default install prefix is not the canonical Windows stage")
        if "DAL_INSTALL_PREFIX" not in text:
            errors.append(f"{relative_path}: does not pass the canonical DAL_INSTALL_PREFIX")


def check_component_build_modes(errors: list[str]) -> None:
    for relative_path, stale_flag in (
        ("dal-python/CMakeLists.txt", "DAL_PYTHON_AS_SUBDIRECTORY"),
        ("dal-excel/CMakeLists.txt", "DAL_EXCEL_AS_SUBDIRECTORY"),
    ):
        if stale_flag in (ROOT / relative_path).read_text(encoding="utf-8"):
            errors.append(f"{relative_path}: subdirectory detection depends on unset {stale_flag}")

    excel_cmake = (ROOT / "dal-excel/CMakeLists.txt").read_text(encoding="utf-8")
    if "${CMAKE_CURRENT_SOURCE_DIR}/.." not in excel_cmake:
        errors.append("dal-excel/CMakeLists.txt: standalone target lacks the repository include root")


def check_windows_generation_and_tests(errors: list[str]) -> None:
    windows_build = (ROOT / "build_windows.bat").read_text(encoding="utf-8")
    windows_workflow = (ROOT / ".github/workflows/cmake-windows.yml").read_text(encoding="utf-8")
    normalizer = "normalize-calibration-generated-enums.cmake"
    if normalizer not in windows_build:
        errors.append("build_windows.bat: code generation bypasses the canonical normalizer")
    if windows_workflow.count(normalizer) < 2:
        errors.append(".github/workflows/cmake-windows.yml: generation jobs bypass the normalizer")
    if "DAL_CPP_BUILD_BENCHMARKS=OFF" not in windows_build or "-LE benchmark" not in windows_build:
        errors.append("build_windows.bat: normal verification must exclude benchmark tests")


def check_benchmark_case_policy(errors: list[str]) -> None:
    contributing = (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8")
    if "Head-only cases are reported as new informational coverage" not in contributing:
        errors.append("CONTRIBUTING.md: benchmark case-set policy does not describe head-only coverage")


def check_python_project_metadata(errors: list[str], metadata: dict) -> None:
    project = metadata["project"]
    if "scikit-build-core==1.0.3" not in metadata["build-system"]["requires"]:
        errors.append("dal-python/pyproject.toml: scikit-build-core must be reproducibly pinned")
    init = (ROOT / "dal-python/src/dal/__init__.py").read_text(encoding="utf-8")
    version_match = re.search(r'^__version__\s*=\s*["\']([^"\']+)["\']\s*$', init, re.MULTILINE)
    source_version = version_match.group(1) if version_match else None
    if source_version != project["version"]:
        errors.append(
            "dal-python: src/dal/__init__.py __version__ must match pyproject.toml project.version"
        )
    if project.get("requires-python") != ">=3.9,<3.14":
        errors.append("dal-python/pyproject.toml: release wheels must target CPython 3.9-3.13")
    classifiers = project.get("classifiers", [])
    implementation_classifier = "Programming Language :: Python :: Implementation :: CPython"
    if implementation_classifier not in classifiers:
        errors.append("dal-python/pyproject.toml: missing CPython implementation classifier")
    expected_versions = {
        f"Programming Language :: Python :: {version}"
        for version in ("3.9", "3.10", "3.11", "3.12", "3.13")
    }
    actual_versions = {
        classifier
        for classifier in classifiers
        if re.fullmatch(r"Programming Language :: Python :: 3\.[0-9]+", classifier)
    }
    if actual_versions != expected_versions:
        errors.append(
            "dal-python/pyproject.toml: version classifiers must be exactly CPython 3.9-3.13"
        )
    if project.get("readme") != "README.md":
        errors.append("dal-python/pyproject.toml: project.readme must publish the component README")


def check_cibuildwheel_config(errors: list[str], metadata: dict) -> None:
    cibuildwheel = metadata["tool"]["cibuildwheel"]
    expected_builds = {"cp39-*", "cp310-*", "cp311-*", "cp312-*", "cp313-*"}
    actual_builds = cibuildwheel.get("build", [])
    if len(actual_builds) != len(set(actual_builds)):
        errors.append("dal-python/pyproject.toml: cibuildwheel selectors must be unique")
    if set(actual_builds) != expected_builds:
        errors.append("dal-python/pyproject.toml: cibuildwheel build matrix must cover cp39-cp313")
    if cibuildwheel.get("linux", {}).get("manylinux-x86_64-image") != "manylinux_2_28":
        errors.append("dal-python/pyproject.toml: Linux wheels must use manylinux_2_28")
    if cibuildwheel.get("linux", {}).get("archs") != ["x86_64"]:
        errors.append("dal-python/pyproject.toml: Linux wheel architecture must be x86_64")
    if cibuildwheel.get("windows", {}).get("archs") != ["AMD64"]:
        errors.append("dal-python/pyproject.toml: Windows wheel architecture must be AMD64")


def check_python_helper_scripts(errors: list[str]) -> None:
    for relative_path in (
        "build_linux.sh",
        "dal-python/build_sdist.sh",
        "dal-python/build_wheel.ps1",
        "dal-python/build_wheel.sh",
        "dal-python/run_tests.ps1",
        "dal-python/run_tests.sh",
    ):
        script = (ROOT / relative_path).read_text(encoding="utf-8")
        if '">=3.9,<3.14"' not in script:
            errors.append(f"{relative_path}: uv interpreter range must match project.requires-python")
        selector = "-Python" if relative_path.endswith(".ps1") else "--python"
        if selector not in script or any(version not in script for version in ("3.9", "3.10", "3.11", "3.12", "3.13")):
            errors.append(f"{relative_path}: exact Python selector must cover 3.9-3.13")
        if "python_compat.py" not in script:
            errors.append(f"{relative_path}: reused environments must use shared interpreter validation")
        if relative_path != "build_linux.sh" and '"scikit-build-core==1.0.3"' not in script:
            errors.append(f"{relative_path}: scikit-build-core must match the build-system pin")


def check_python_release_action(errors: list[str]) -> None:
    workflow_path = ROOT / ".github/workflows/dal-python-release.yml"
    workflow = workflow_path.read_text(encoding="utf-8")
    required_fragments = (
        "dal-python-v*",
        "workflow_dispatch:",
        "if: github.ref_type == 'tag'",
        "name: pypi",
        "id-token: write",
        "packages-dir: wheelhouse",
        "--check-pypi",
        "dal-python/scripts/verify_release.py",
        ".github/scripts/check_dal_python_syntax.py",
        "dal-python/scripts/smoke_installed_wheel.py",
        "uv run --isolated --no-project --python 3.9",
        "astral-sh/setup-uv@c771a70e6277c0a99b617c7a806ffedaca235ff9",
        "pypa/cibuildwheel@1828c10ab37f080699c7b81cea34097c684a7074",
        'test "$(git rev-parse FETCH_HEAD)" = "${GITHUB_SHA}"',
        'git cat-file -t "${GITHUB_REF}"',
    )
    for fragment in required_fragments:
        if fragment not in workflow:
            errors.append(f".github/workflows/dal-python-release.yml: missing {fragment!r}")
    for forbidden in ("skip-existing", "PYPI_API_TOKEN", "password:", "--sdist"):
        if forbidden in workflow:
            errors.append(f".github/workflows/dal-python-release.yml: forbidden {forbidden!r}")
    for line in workflow.splitlines():
        if "uses:" not in line:
            continue
        reference = line.split("@", maxsplit=1)[-1].split(maxsplit=1)[0]
        if re.fullmatch(r"[0-9a-f]{40}", reference) is None:
            errors.append(
                ".github/workflows/dal-python-release.yml: actions must be pinned to full SHAs"
            )


def release_expression_values(
    workflow: str, name: str, errors: list[str]
) -> tuple[str, str] | None:
    match = re.search(
        rf"^\s*{re.escape(name)}:\s*\$\{{\{{\s*github\.event_name\s*==\s*"
        rf"'pull_request'\s*&&\s*'([^']*)'\s*\|\|\s*'([^']*)'\s*}}}}\s*$",
        workflow,
        flags=re.MULTILINE,
    )
    if match is None:
        errors.append(
            f".github/workflows/dal-python-release.yml: {name} event projection was not found"
        )
        return None
    return match.group(1), match.group(2)


def parse_build_projection(
    value: str, event: str, errors: list[str]
) -> tuple[str, ...] | None:
    selectors = value.split()
    if not selectors or len(selectors) != len(set(selectors)):
        errors.append(
            f"{event} build projection must be nonempty and unique: {selectors!r}"
        )
        return None
    malformed = [
        selector
        for selector in selectors
        if re.fullmatch(r"cp[1-9][0-9]+-\*", selector) is None
    ]
    if malformed:
        errors.append(
            f"{event} build projection has malformed selectors: {malformed!r}"
        )
        return None
    return tuple(selector.removesuffix("-*") for selector in selectors)


def verifier_projection_parts_are_valid(value: str, tags: list[str]) -> bool:
    if not value:
        return False
    if any(not tag or tag != tag.strip() for tag in tags):
        return False
    return len(tags) == len(set(tags))


def parse_verify_projection(
    value: str, event: str, errors: list[str]
) -> tuple[str, ...] | None:
    tags = value.split(",")
    if not verifier_projection_parts_are_valid(value, tags):
        errors.append(
            f"{event} verifier projection must be nonempty and unique: {tags!r}"
        )
        return None
    malformed = [tag for tag in tags if re.fullmatch(r"cp[1-9][0-9]+", tag) is None]
    if malformed:
        errors.append(
            f"{event} verifier projection has malformed selectors: {malformed!r}"
        )
        return None
    return tuple(tags)


def release_projection_mismatches(
    build: tuple[str, ...], verify: tuple[str, ...], expected: set[str]
) -> bool:
    return (
        set(build) != expected or set(verify) != expected or set(build) != set(verify)
    )


def release_projection_target_count_mismatches(
    build: tuple[str, ...], verify: tuple[str, ...], expected_targets: int
) -> bool:
    return len(build) * 2 != expected_targets or len(verify) * 2 != expected_targets


def check_release_projection_contract(
    event: str,
    raw_build: str,
    raw_verify: str,
    expected: set[str],
    expected_targets: int,
    errors: list[str],
) -> None:
    build = parse_build_projection(raw_build, event, errors)
    verify = parse_verify_projection(raw_verify, event, errors)
    if build is None or verify is None:
        return
    if release_projection_mismatches(build, verify, expected):
        errors.append(
            f".github/workflows/dal-python-release.yml {event} projection mismatch: "
            f"build={list(build)!r}, verify={list(verify)!r}, expected={sorted(expected)!r}"
        )
    if release_projection_target_count_mismatches(build, verify, expected_targets):
        errors.append(
            f".github/workflows/dal-python-release.yml {event} projection must yield "
            f"{expected_targets} targets"
        )


def check_python_release_projections(
    errors: list[str], metadata: dict, workflow: str
) -> None:
    configured_selectors = metadata["tool"]["cibuildwheel"].get("build", [])
    configured_tags = {selector.removesuffix("-*") for selector in configured_selectors}
    build_values = release_expression_values(workflow, "CIBW_BUILD", errors)
    verify_values = release_expression_values(workflow, "EXPECTED_PYTHON", errors)
    if build_values is None or verify_values is None:
        return
    contracts = (
        ("pull_request", build_values[0], verify_values[0], {"cp39", "cp313"}, 4),
        ("manual/tag", build_values[1], verify_values[1], configured_tags, 10),
    )
    for event, raw_build, raw_verify, expected, expected_targets in contracts:
        check_release_projection_contract(
            event, raw_build, raw_verify, expected, expected_targets, errors
        )


def check_stale_python_release_texts(
    current_docs: dict[str, str], errors: list[str]
) -> None:
    stale_patterns = (
        r"\b3\.10(?:-|–|—|\s+to\s+|\s+through\s+)3\.13\b",
        r"\beight wheels\b",
    )
    for document, text in current_docs.items():
        for pattern in stale_patterns:
            if re.search(pattern, text, flags=re.IGNORECASE):
                errors.append(
                    f"{document}: stale DAL Python compatibility or wheel-count claim"
                )


def check_required_python_release_texts(
    texts: dict[str, str], errors: list[str]
) -> None:
    required = {
        "dal-python/README.md": (
            "CPython 3.9-3.13",
            ">=3.9,<3.14",
            "`manylinux_2_28_x86_64`",
            "`win_amd64`",
            "`cp39-cp39`",
            "`cp313-cp313`",
            "four wheels",
            "ten wheels",
        ),
        "docs/installation.md": (
            "CPython 3.9-3.13",
            ">=3.9,<3.14",
            "`cp39-cp39`",
            "`cp313-cp313`",
            "four wheels",
            "ten wheels",
        ),
        "CHANGELOG.md": (
            "CPython 3.9-3.13",
            "Requires-Python: >=3.9,<3.14",
            "ten CPython-specific wheels",
        ),
    }
    for document, fragments in required.items():
        for fragment in fragments:
            if fragment not in texts[document]:
                errors.append(
                    f"{document}: missing DAL Python release contract {fragment!r}"
                )


def check_python_release_selector_texts(selector_docs: str, errors: list[str]) -> None:
    for command in (
        "build_linux.sh --python 3.9",
        "build_sdist.sh --python 3.9",
        "build_wheel.sh --python 3.9",
        "build_wheel.ps1 -Python 3.9",
        "run_tests.sh --python 3.9",
        "run_tests.ps1 -Python 3.9",
    ):
        if command not in selector_docs:
            errors.append(
                f"DAL Python public docs: missing local selector example {command!r}"
            )


def check_python_release_exclusion_texts(readme: str, errors: list[str]) -> None:
    for exclusion in (
        "CPython 3.14",
        "macOS",
        "Linux ARM",
        "musllinux",
        "PyPy",
        "free-threaded CPython",
        "source distributions",
    ):
        if exclusion not in readme:
            errors.append(
                f"dal-python/README.md: missing release exclusion {exclusion!r}"
            )


def check_python_release_document_texts(
    readme: str, installation: str, changelog: str, errors: list[str]
) -> None:
    current_docs = {
        "dal-python/README.md": readme,
        "docs/installation.md": installation,
    }
    texts = {**current_docs, "CHANGELOG.md": changelog}
    check_stale_python_release_texts(current_docs, errors)
    check_required_python_release_texts(texts, errors)
    check_python_release_selector_texts(f"{readme}\n{installation}", errors)
    check_python_release_exclusion_texts(readme, errors)


def check_python_release_documentation(errors: list[str]) -> None:
    check_python_release_document_texts(
        (ROOT / "dal-python/README.md").read_text(encoding="utf-8"),
        (ROOT / "docs/installation.md").read_text(encoding="utf-8"),
        (ROOT / "CHANGELOG.md").read_text(encoding="utf-8"),
        errors,
    )


def check_python_release_workflow(errors: list[str]) -> None:
    with (ROOT / "dal-python/pyproject.toml").open("rb") as stream:
        metadata = tomllib.load(stream)
    check_python_project_metadata(errors, metadata)
    check_cibuildwheel_config(errors, metadata)
    check_python_helper_scripts(errors)
    check_python_release_action(errors)
    workflow = (ROOT / ".github/workflows/dal-python-release.yml").read_text(encoding="utf-8")
    check_python_release_projections(errors, metadata, workflow)
    check_python_release_documentation(errors)


def check_repository_workflows(errors: list[str]) -> None:
    check_windows_python_helpers(errors)
    check_component_build_modes(errors)
    check_windows_generation_and_tests(errors)
    check_benchmark_case_policy(errors)
    check_python_release_workflow(errors)


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
    check_current_state_doc_locations(errors)
    check_ci_compiler_inventory(errors)
    check_repository_workflows(errors)

    if errors:
        print("Documentation checks failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(f"Documentation checks passed for {len(ALL_DOCS)} Markdown files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
