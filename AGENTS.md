# AGENTS.md

Last updated: 2026-07-24

Codex-native guidance for this repository. This file is intentionally separate from
`CLAUDE.md` and `.claude/`; do not edit the Claude originals unless the user explicitly asks.

`CLAUDE.md` is canonical only for shared build/test commands and the architecture map. Codex
routing, workflows, references, and durable outputs are owned under `.codex/`.

## Codex Surfaces

- `.codex/agents/` registers the named Derivatives Algorithms Library (DAL) custom-agent specialists.
- `.codex/skills/` owns reusable workflows and their output contracts.
- `.codex/skills/*/references/` owns the shared style, test, web, git, and team conventions.
- `.codex/artifacts/` owns durable specifications, designs, API notes, critiques, reviews,
  plans, and performance reports.

## Repository Shape

This is a C++17 quantitative finance workspace with Automatic Adjoint Differentiation (AAD)
support. The dependency direction is `dal-cpp <- dal-public <- {dal-python, dal-excel}`;
`dal-web/` (FastAPI backend plus React/Vite frontend) is not built by CMake. The shared
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

## Codex References

- C++ style: [code-style.md](.codex/skills/dal-agent-team/references/code-style.md).
- Machinist enum generation: [code-style.md](.codex/skills/dal-agent-team/references/code-style.md#enums).
- Google Test conventions: [unit-test-style.md](.codex/skills/dal-agent-team/references/unit-test-style.md).
- Test execution workflow: [dal-tester](.codex/skills/dal-tester/SKILL.md).
- Test authoring workflow: [dal-tester](.codex/skills/dal-tester/SKILL.md).
- Branch naming: [git-commit-pr.md](.codex/skills/dal-agent-team/references/git-commit-pr.md).
- Commit conventions: [git-commit-pr.md](.codex/skills/dal-agent-team/references/git-commit-pr.md).
- Pull-request publication: [publish-workflow.md](.codex/skills/dal-git-pr/references/publish-workflow.md).
- Web operations: [operations.md](.codex/skills/dal-web/references/operations.md).
- Web backend rules: [backend-style.md](.codex/skills/dal-web/references/backend-style.md).
- Web UI design: [design-system.md](.codex/skills/dal-web/references/design-system.md).

## Work Style

- Preserve user changes unless the user explicitly requests a revert.
- Inspect the working tree before editing.
- Leave unrelated work unchanged.
- Prefer small, test-driven changes for C++ behavior: red, green, refactor.
- Use `apply_patch` for manual edits.
- Follow `.clang-format`.
- For reviews, lead with findings and file/line references.
- Keep published docs current-state only.
- Put historical context in `CHANGELOG.md` or a `.codex/artifacts/` record.

## Skill Routing

- Use a named custom agent from `.codex/agents/` only when the user authorizes agent execution.
- Treat a custom-agent registration as the specialist identity, not a workflow.
- Use the matching `.codex/skills/dal-*/` workflow in the current session when delegation is
  not authorized.
- Treat skills as the owners of reusable behavior.
- Load the references linked by the selected skill.
- Use `dal-agent-team` for role selection or the end-to-end DAL pipeline.
- Use the individual role skills for focused specialist work.
- Use `dal-web` for web work.
- Use `dal-git-pr` for Git and pull-request workflows.
