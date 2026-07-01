# DAL Agent Team

A coordinated team of specialist agents for the DAL (Derivatives Algorithms Library) C++
quantitative finance project. Each agent owns one phase of the spec → design → critique →
implement → review → document pipeline. The orchestrator routes work between them.

## Team Roster

| Role         | Agent              | Color  | Reads                                                         | Writes                                                                                     |
|--------------|--------------------|--------|---------------------------------------------------------------|--------------------------------------------------------------------------------------------|
| Orchestrator | `dal-orchestrator` | purple | GitHub issues, all artifacts                                  | task list, PRs                                                                             |
| Spec writer  | `dal-spec-writer`  | orange | issues, methodology, rules                                    | `.claude/specs/<slug>.md`                                                                  |
| API designer | `dal-api-designer` | pink   | spec, design, public headers                                  | `.claude/api-notes/<slug>.md`                                                              |
| Critic       | `dal-critic`       | red    | spec, design, api-note                                        | `.claude/critiques/<slug>.md`                                                              |
| Implementer  | `dal-implementer`  | green  | spec, design, api-note, critique                              | source code, tests, TDD in worktree                                                        |
| Tester       | `dal-tester`       | cyan   | source under-test, conventions                                | `dal-cpp/tests/<module>/*` and, for web scope, `dal-web/frontend/tests/e2e/*`, in worktree |
| Reviewer     | `dal-reviewer`     | amber  | PR diff, all upstream artifacts                               | review report, optional merge                                                              |
| Performancer | `dal-performancer` | yellow | finished impl, benchmark binaries, baseline `*_perf` output   | perf-regression report, benchmark-coverage advisory                                        |
| Doc writer   | `dal-doc-writer`   | teal   | current source, CLAUDE.md, docs                               | `docs/` and `CHANGELOG.md`                                                                 |


## Workflow

```
issue ──► spec-writer ──► api-designer ──► critic
                          (if public)        │
                                            ▼
            reviewer   ◄──── implementer (+ tester) ◄──────┘
            performancer
                 │
                 ▼
            doc-writer (reconcile docs/ + CHANGELOG.md)
                 │
                 ▼
              merged PR
```

The orchestrator is the only agent that decides which steps to skip. Most issues take a
subset of the pipeline (see `dal-orchestrator.md` for the routing table).

## Artifact Layout

| Path                 | Owner       | Purpose                                                        |
|----------------------|-------------|----------------------------------------------------------------|
| `.claude/specs/`     | spec writer | testable requirement specifications (created on demand)        |
| `.claude/api-notes/` | api designer| public-API surface notes (created on demand)                   |
| `.claude/critiques/` | critic      | adversarial reviews of specs and api-notes (created on demand) |
| `docs/`              | doc writer  | normative quant method docs and index (referenced by all agents) |
| `CHANGELOG.md`       | doc writer  | dated log of fundamental changes (single-version history)      |
| `.claude/rules/`     | (existing)  | normative coding/test/git conventions                          |

Filenames share a single kebab-case slug derived from the issue title, so an issue traces
through `specs/log-linear-interp.md → api-notes/log-linear-interp.md → ...` end-to-end.

## How to Invoke the Team

- **End-to-end on a GitHub issue.** "Use `dal-orchestrator` to handle issue #57." The
  orchestrator fetches the issue, decomposes the work, and delegates to teammates.
- **A single specialist.** Address the role directly: "Use `dal-spec-writer` to spec the
  multi-curve refactor described in issue #42."
- **Adversarial review of an existing plan.** "Use `dal-critic` on the spec at
  `.claude/specs/foo.md`."

## Conventions Each Agent Honors

- `.claude/rules/code-style.md` — naming, headers, includes, error handling, enums (Machinist)
- `.claude/rules/unit-test-style.md` — Google Test patterns, assertions, suite naming
- `.claude/rules/git-commit-pr.md` — branch naming, commit message format, PR template
- `docs/methodology/*.md` — domain vocabulary; quant claims must match these docs

## Team Working Agreements

Two practices are mandatory for every agent that changes files in the repository
(`dal-implementer`, `dal-tester`, `dal-doc-writer`, and `dal-performancer` when it adds a benchmark;
the `dal-reviewer` also reviews inside a worktree):

- **Worktree isolation.** Enter an isolated git worktree (`EnterWorktree`) before creating or
  editing any file. All edits, builds, iteration, and the commit/PR happen inside it, keeping
  the main working tree clean. The planning agents (spec writer, API designer, critic)
  write only into the shared `.claude/` artifact directories (created on demand) and do not need a worktree.
- **Test-driven development (TDD).** The implementer works strictly red → green → refactor:
  write a failing test for the next behavior, confirm it fails for the right reason, write the
  minimum code to pass, then refactor while green. Production code is never written ahead of a
  test that demands it. The doc writer is exempt from TDD (there is no code to test), but still
  works in a worktree.

## Hand-off Etiquette

- One agent at a time per artifact. Don't fan out the same artifact to two agents in parallel.
- Self-contained prompts. The teammate agent doesn't see the parent conversation, so the
  invocation must include all paths, decisions, and acceptance criteria it needs.
- Verify before advancing. The orchestrator checks each artifact exists and has the expected
  shape (acceptance criteria, file map, verdict, etc.) before the next step starts.
- A `Block` verdict from the critic routes work back to the upstream author, not forward to
  the implementer.
