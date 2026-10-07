# AGENTS.md

Last updated: 2026-10-07

Codex-native guidance for this repository. This file is intentionally separate from
`CLAUDE.md` and `.claude/`; do not edit the Claude originals unless the user explicitly asks.

`CLAUDE.md` is canonical only for shared build/test commands and the architecture map. Codex
routing, workflows, references, and durable outputs are owned under `.codex/`.

## Codex Surfaces

- `.codex/agents/` registers named Derivatives Algorithms Library (DAL) specialists and owns
  their complete role contracts.
- These TOMLs also own the shared DAL role contracts mirrored in Claude agent
  bodies and Multica agent instructions. Preserve platform registration metadata;
  synchronize other platforms only when the task authorizes those changes.
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

## Performance Acceptance

These rules apply to every development task in this repository.

- Select performance cases from the changed code and its actual callers. Record a
  short mapping from changed paths to selected cases and explain coverage exclusions.
- Use the smallest set covering affected algorithms, public entry points and relevant
  size/mode boundaries. Do not run the full benchmark or parameter matrix for every
  edit, PR or merge. Shared headers require caller analysis, not automatic full coverage.
- During repair, repeat only affected cases. Expand coverage when a new failure,
  dependency change or specific coverage gap justifies it; record the reason first.
- Preserve calibrated sampling, noise controls and regression thresholds for selected
  cases. Reduce case count rather than weakening evidence for each case.
- Reuse accepted evidence only when source/dependency/configuration provenance and
  executable identity establish applicability. Rebuild affected binaries; do not use
  stale binaries or claim omitted cases passed.
- Scheduled full-suite monitoring and required exact-head CI remain applicable.
  Document scope decisions in the PR using [benchmark-workflow.md](.codex/references/benchmark-workflow.md).

## Work Style

- Preserve user changes unless the user explicitly requests a revert.
- Inspect the working tree before editing.
- Leave unrelated work unchanged.
- Prefer small, test-driven changes for C++ behavior.
- Follow the red-green-refactor cycle.
- Use `apply_patch` for manual edits.
- Follow `.clang-format`.
- Keep code comments brief and limited to local constraints or non-obvious reasons. Do not
  reference documentation files from code; documentation may reference source files.
- For reviews, lead with findings and file/line references.
- Keep published docs current-state only.
- Favor readable documentation over exhaustive coverage. Keep reusable explanations in
  focused guides and use direct links instead of repeating them in code comments.
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
