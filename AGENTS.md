# AGENTS.md

Last updated: 2026-08-15

Codex-native guidance for this repository. This file is intentionally separate from
`CLAUDE.md` and `.claude/`; do not edit the Claude originals unless the user explicitly asks.

`CLAUDE.md` is canonical only for shared build/test commands and the architecture map. Codex
routing, workflows, references, and durable outputs are owned under `.codex/`.

## Codex Surfaces

- `.codex/agents/` registers named Derivatives Algorithms Library (DAL) specialists and owns
  their complete role contracts.
- `.codex/references/` owns reusable C++, test, review, performance, and Git conventions.
- `.codex/skills/` owns reusable non-agent workflows for Git/PR packaging.
- `.codex/artifacts/` owns active specifications, designs, API notes, critiques, reviews,
  plans, and performance reports.

## Repository Shape

This is a C++17 quantitative finance workspace with Automatic Adjoint Differentiation (AAD)
support. The dependency direction is `dal-cpp <- dal-public <- {dal-python, dal-excel}`. The shared
architecture map is in [CLAUDE.md](CLAUDE.md#architecture), with the published overview in
[docs/architecture.md](docs/architecture.md).

## Build And Test

Shared C++ build commands, CMake options, and test invocations remain canonical in
[CLAUDE.md](CLAUDE.md#build-commands) and [CLAUDE.md](CLAUDE.md#running-tests). The published
setup guide is [docs/installation.md](docs/installation.md).

```bash
bash ./build_linux.sh
ctest --test-dir build/Release-linux --output-on-failure
```

The [DAL unit-test contract](.codex/references/unit-test-style.md) defines
repository-specific Google Test rules.
The [tester agent contract](.codex/agents/dal-tester.toml) covers test execution and authoring.

## Codex References

**C++ style:** [code-style.md](.codex/references/code-style.md).

**Machinist enum generation:** [code-style.md](.codex/references/code-style.md#enums).

**Branch naming:** [git-commit-pr.md](.codex/references/git-commit-pr.md).

**Commit conventions:** [git-commit-pr.md](.codex/references/git-commit-pr.md).

**Pull-request publication:** [publish-workflow.md](.codex/skills/dal-git-pr/references/publish-workflow.md).

## Work Style

- Preserve user changes unless the user explicitly requests a revert.
- Inspect the working tree before editing.
- Leave unrelated work unchanged.
- Prefer small, test-driven changes for C++ behavior.
- Follow the red-green-refactor cycle.
- Use `apply_patch` for manual edits.
- Follow `.clang-format`.
- For reviews, lead with findings and file/line references.
- Keep published docs current-state only.
- Put public history in `CHANGELOG.md`, use Git history for delivery records, and reserve
  `.codex/artifacts/` for work that is still active.

## Agent And Skill Routing

- Use a named custom agent from `.codex/agents/` only when the user authorizes agent execution.
- Treat each custom-agent registration as both the specialist identity and its complete role contract.
- Without delegation authority, read the matching agent registration file and follow its contract locally.
- Use `dal-orchestrator` for role selection or the end-to-end DAL pipeline.
- Load references named by the selected agent contract.
- Use `dal-git-pr` for Git and pull-request workflows.
- Keep artifacts only while they control active work; use Git history for completed work.
