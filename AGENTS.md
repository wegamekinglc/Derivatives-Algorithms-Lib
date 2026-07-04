# AGENTS.md

Codex-native guidance for working in this repository. This file is intentionally separate from
`CLAUDE.md` and `.claude/`; do not edit the Claude originals unless the user explicitly asks.

## Codex Artifacts

- Project-local Codex skills are stored under `.codex/skills/`.
- To make these skills globally discoverable outside this repo, copy or sync each folder under `.codex/skills/` into `~/.codex/skills/`.
- Prefer the new Codex skills over the Claude agent files when a user asks for implementation, tests, reviews, docs, web work, or PR packaging.
- The individual Claude roles from `.claude/agents/` have same-name Codex skill equivalents under `.codex/skills/dal-*`.

## Repository Shape

This is a C++17 quantitative finance workspace with AAD support.

- `dal-cpp/` is the core library and always builds.
- `dal-public/` wraps the core C++ API.
- `dal-python/` contains pybind11 bindings and the Python package.
- `dal-excel/` contains the Windows Excel add-in.
- `dal-web/` is a FastAPI backend plus React/Vite frontend and is not built by CMake.

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

- Preserve user changes. Inspect the working tree before edits and never revert unrelated work.
- Prefer small, test-driven changes for C++ behavior: red, green, refactor.
- Use `apply_patch` for manual edits.
- Follow `.clang-format` and the conventions captured in `.codex/skills/dal-agent-team/references/shared-rules.md`.
- For reviews, lead with findings and file/line references.
- Keep docs current-state only. Put historical context only in `CHANGELOG.md`.

## Skill Routing

- Use `dal-agent-team` for role routing or end-to-end team flow.
- Use same-name role skills for Claude-agent equivalents: `dal-orchestrator`, `dal-spec-writer`, `dal-api-designer`, `dal-critic`, `dal-implementer`, `dal-tester`, `dal-reviewer`, `dal-performancer`, `dal-simplifier`, and `dal-doc-writer`.
- Use `dal-web` for starting/stopping the web app, backend async rules, frontend e2e, and web UI design.
- Use `dal-git-pr` for committing, pushing, and opening or updating PRs.
