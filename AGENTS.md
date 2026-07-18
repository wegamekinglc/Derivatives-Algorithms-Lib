# AGENTS.md

Last updated: 2026-07-18

Codex-native guidance for working in this repository. This file is intentionally separate from
`CLAUDE.md` and `.claude/`; do not edit the Claude originals unless the user explicitly asks.

[CLAUDE.md](CLAUDE.md) is the agent-facing source of truth for build/test commands, workspace
CMake options, and the architecture map. This file keeps Codex-specific routing and work style,
and links back instead of duplicating.

## Codex Artifacts

| Priority  | Guidance                                                                                                                                     |
|-----------|----------------------------------------------------------------------------------------------------------------------------------------------|
| Reference | Project-local Codex skills are stored under `.codex/skills/`.                                                                                |
| Optional  | To make these skills globally discoverable outside this repo, copy or sync each folder under `.codex/skills/` into `~/.codex/skills/`.       |
| Default   | Prefer the new Codex skills over the Claude agent files when a user asks for implementation, tests, reviews, docs, web work, or PR packaging. |
| Reference | The individual Claude roles from `.claude/agents/` have same-name Codex skill equivalents under `.codex/skills/dal-*`.                       |

## Repository Shape

This is a C++17 quantitative finance workspace with Automatic Adjoint Differentiation (AAD) support.
The dependency direction is `dal-cpp <- dal-public <- {dal-python, dal-excel}`; `dal-web/` (FastAPI
backend plus React/Vite frontend) is not built by CMake. The sub-project map lives in
[CLAUDE.md](CLAUDE.md#architecture) and the published [architecture guide](docs/architecture.md).

## Build And Test

Build commands, workspace CMake options, and test invocations are canonical in
[CLAUDE.md](CLAUDE.md#build-commands) and [CLAUDE.md](CLAUDE.md#running-tests); the published
setup guide is [docs/installation.md](docs/installation.md). The short form:

```bash
bash ./build_linux.sh
ctest --test-dir build/Release-linux --output-on-failure
```

If enum Machinist markup changes, regenerate both core and Excel auto files before building —
see [Machinist enums](.codex/skills/dal-agent-team/references/shared-rules.md#machinist-enums).

## Work Style

| Priority  | Guidance                                                                                                                                       |
|-----------|------------------------------------------------------------------------------------------------------------------------------------------------|
| Mandatory | Preserve user changes unless the user explicitly requests a revert; inspect the working tree before edits and leave unrelated work alone.       |
| Default   | Prefer small, test-driven changes for C++ behavior: red, green, refactor.                                                                      |
| Default   | Use `apply_patch` for manual edits.                                                                                                            |
| Mandatory | Follow `.clang-format` and the conventions captured in `.codex/skills/dal-agent-team/references/shared-rules.md`.                              |
| Mandatory | For reviews, lead with findings and file/line references.                                                                                      |
| Mandatory | Keep docs current-state only. Put historical context only in `CHANGELOG.md`.                                                                   |

## Skill Routing

| Priority | Skill or group             | Use for                                                                                                                                                                   |
|----------|----------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Default  | `dal-agent-team`           | Role routing or end-to-end team flow.                                                                                                                                     |
| Default  | Same-name role skills      | Claude-agent equivalents: `dal-orchestrator`, `dal-spec-writer`, `dal-api-designer`, `dal-critic`, `dal-implementer`, `dal-tester`, `dal-reviewer`, `dal-performancer`, `dal-simplifier`, and `dal-doc-writer`. |
| Default  | `dal-web`                  | Starting/stopping the web app, backend async rules, frontend e2e, and web UI design.                                                                                      |
| Default  | `dal-git-pr`               | Committing, pushing, and opening or updating PRs.                                                                                                                         |
