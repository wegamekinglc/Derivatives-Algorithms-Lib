# AGENTS.md

Last updated: 2026-07-04

Codex-native guidance for working in this repository. This file is intentionally separate from
`CLAUDE.md` and `.claude/`; do not edit the Claude originals unless the user explicitly asks.

## Codex Artifacts

- **Reference:** Project-local Codex skills are stored under `.codex/skills/`.
- **Optional:** To make these skills globally discoverable outside this repo, copy or sync each folder under `.codex/skills/` into `~/.codex/skills/`.
- **Default:** Prefer the new Codex skills over the Claude agent files when a user asks for implementation, tests, reviews, docs, web work, or PR packaging.
- **Reference:** The individual Claude roles from `.claude/agents/` have same-name Codex skill equivalents under `.codex/skills/dal-*`.

## Repository Shape

This is a C++17 quantitative finance workspace with Automatic Adjoint Differentiation (AAD) support.

- **Reference:** `dal-cpp/` is the core library and is enabled in the normal CMake workspace.
- **Reference:** `dal-public/` wraps the core C++ API.
- **Reference:** `dal-python/` contains pybind11 bindings and the Python package.
- **Reference:** `dal-excel/` contains the Windows Excel add-in.
- **Reference:** `dal-web/` is a FastAPI backend plus React/Vite frontend and is not built by CMake.

The dependency direction is `dal-cpp <- dal-public <- {dal-python, dal-excel}`.

## Build And Test

Use the existing project scripts and presets:

```bash
bash ./build_linux.sh
```

```bash
mkdir -p build
cd build
cmake --preset=Release-linux ..
make -j$(nproc)
make install
```

Run tests from the installed binaries or CTest:

```bash
(cd build && ctest --output-on-failure)
bin/dal_cpp_tests
bin/dal_public_tests
bin/dal_cpp_tests --gtest_filter=<SuiteName>.*
```

If enum Machinist markup changes, regenerate both core and Excel auto files before building.

## Work Style

- **Mandatory:** Preserve user changes unless the user explicitly requests a revert; inspect the working tree before edits and leave unrelated work alone.
- **Default:** Prefer small, test-driven changes for C++ behavior: red, green, refactor.
- **Default:** Use `apply_patch` for manual edits.
- **Mandatory:** Follow `.clang-format` and the conventions captured in `.codex/skills/dal-agent-team/references/shared-rules.md`.
- **Mandatory:** For reviews, lead with findings and file/line references.
- **Mandatory:** Keep docs current-state only. Put historical context only in `CHANGELOG.md`.

## Skill Routing

- **Default:** Use `dal-agent-team` for role routing or end-to-end team flow.
- **Default:** Use same-name role skills for Claude-agent equivalents: `dal-orchestrator`, `dal-spec-writer`, `dal-api-designer`, `dal-critic`, `dal-implementer`, `dal-tester`, `dal-reviewer`, `dal-performancer`, `dal-simplifier`, and `dal-doc-writer`.
- **Default:** Use `dal-web` for starting/stopping the web app, backend async rules, frontend e2e, and web UI design.
- **Default:** Use `dal-git-pr` for committing, pushing, and opening or updating PRs.
