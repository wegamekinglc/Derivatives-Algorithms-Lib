# DAL Agent Team

DAL has ten specialist roles. Their shared behavior contracts are owned by
`.codex/agents/*.toml`; each matching Claude registration keeps the same
description and behavior in Markdown with YAML frontmatter. Claude retains its
native `model: inherit` and color metadata. These registration differences do
not grant additional editing, delegation, publication, or merge permissions.

## Roles and Routing

| Agent              | Responsibility                                           |
|--------------------|----------------------------------------------------------|
| `dal-orchestrator` | Gather context, select roles, coordinate, and report     |
| `dal-spec-writer`  | Define precise, testable requirements                    |
| `dal-api-designer` | Design C++, Python, Excel, and example surfaces          |
| `dal-critic`       | Review specifications and designs before implementation  |
| `dal-implementer`  | Implement approved behavior with red-green-refactor      |
| `dal-tester`       | Run, write, and repair tests as distinct activities      |
| `dal-reviewer`     | Review full changed files, evidence, and merge readiness |
| `dal-doc-writer`   | Reconcile current-state docs and judge changelog scope   |
| `dal-performancer` | Measure regressions and advise on benchmark coverage     |
| `dal-simplifier`   | Report duplication; apply fixes only when requested      |

The [orchestrator contract](dal-orchestrator.md) defines the route:

- Unclear features: spec writer → critic → implementer → tester → reviewer → doc writer.
- Public surfaces: insert API designer after specification and before critique.
- Clear bug fixes: implementer → tester → reviewer → doc writer.
- Pure coverage: tester → reviewer.
- Pure documentation: doc writer → reviewer.
- Performance and simplification are optional post-correctness sidecars.

Every code change needs a reviewer. The doc writer decides whether docs or
`CHANGELOG.md` need changes, except for pure tests and behavior-preserving refactors.
Task-specific instructions may select a shorter applicable route.

## Authorization and Handoff

Delegate only when the user or applicable repository guidance authorizes agent
execution. Otherwise read the matching contract and follow it in the current
session. Use the host's available tools to read issue/PR context, branch state,
and active artifacts; the orchestrator may inspect this evidence while remaining
a dispatcher.

Each delegation includes a self-contained scope, disjoint write ownership,
acceptance criteria, upstream decisions, and required evidence. Dependent work
runs sequentially; only independent work may run in parallel. Report completed
scope, branch/commit/PR, verification, remaining blockers, and open questions.

The reviewer reports findings by default. Submitting a GitHub review, resolving
threads, changing a PR, and merging each require explicit authorization. The
performancer does not edit production code or benchmarks; implementation goes
to the implementer. The simplifier edits only in explicitly requested apply mode.

## Artifacts and References

Active work uses the shared [Codex artifact surface](../../.codex/artifacts/README.md):
`specs/`, `designs/`, `api-notes/`, `critiques/`, `reviews/`, `plans/`, and performance
reports under `.codex/artifacts/`. Create a durable document only when it controls
active work, then retire it when the current-state outcome is documented.
Existing `.claude/specs/`, `.claude/designs/`, `.claude/api-notes/`, and
`.claude/critiques/` documents marked as implemented history are historical
context, not pending instructions or assertions about current paths.

Shared build/test commands and architecture remain in [CLAUDE.md](../../CLAUDE.md).
Coding, unit-test, and Git conventions are mirrored byte-for-byte between
`.claude/rules/` and `.codex/references/`. Specialist contracts link the references
they require. Claude keeps its four user-invocable skills; Codex exposes
`dal-git-pr` and uses references for test and style workflows.

Inspect the working tree before editing and preserve unrelated changes. Use an
isolated checkout or worktree when work would otherwise conflict; Claude's
`EnterWorktree` is a host-specific way to obtain that isolation. Follow
[AGENTS.md](../../AGENTS.md) and the authorized task scope. Publishing follows
the [Git/PR workflow](../skills/dal-commit-and-pr/SKILL.md).
