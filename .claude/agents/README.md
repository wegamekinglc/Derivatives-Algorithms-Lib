# DAL Agent Team

A coordinated team of specialist agents for the DAL (Derivatives Algorithms Library) C++
quantitative finance project. Each agent owns one phase of the spec → design → critique →
implement → review pipeline. The orchestrator routes work between them.

## Team Roster

| Role         | Agent              | Color  | Reads                            | Writes                             |
|--------------|--------------------|--------|----------------------------------|------------------------------------|
| Orchestrator | `dal-orchestrator` | purple | GitHub issues, all artifacts     | task list, PRs                     |
| Spec writer  | `dal-spec-writer`  | orange | issues, methodology, rules       | `.claude/specs/<slug>.md`          |
| Architect    | `dal-architect`    | blue   | spec, codebase, methodology      | `.claude/designs/<slug>.md`        |
| API designer | `dal-api-designer` | pink   | spec, design, public headers     | `.claude/api-notes/<slug>.md`      |
| Critic       | `dal-critic`       | red    | spec, design, api-note           | `.claude/critiques/<slug>.md`      |
| Implementer  | `dal-implementer`  | green  | spec, design, api-note, critique | source code, tests, in worktree    |
| Tester       | `dal-tester`       | cyan   | source under-test, conventions   | additional `tests/<module>/*` code |
| Reviewer     | `dal-reviewer`     | amber  | PR diff, all upstream artifacts  | review report, optional merge      |

## Workflow

```
issue ──► spec-writer ──► architect ──► api-designer ──► critic
                                            (if public)        │
                                                               ▼
                  reviewer ◄──── implementer (+ tester) ◄──────┘
                       │
                       ▼
                    merged PR
```

The orchestrator is the only agent that decides which steps to skip. Most issues take a
subset of the pipeline (see `dal-orchestrator.md` for the routing table).

## Artifact Layout

| Path                   | Owner          | Purpose                                                   |
|------------------------|----------------|-----------------------------------------------------------|
| `.claude/specs/`       | spec writer    | testable requirement specifications                       |
| `.claude/designs/`     | architect      | technical designs with file map and algorithm choice      |
| `.claude/api-notes/`   | api designer   | public-API surface notes (signatures, examples, errors)   |
| `.claude/critiques/`   | critic         | adversarial reviews of specs, designs, and api-notes      |
| `.claude/methodology/` | (existing)     | normative quant method docs (referenced by all agents)    |
| `.claude/rules/`       | (existing)     | normative coding/test/git conventions                     |

Filenames share a single kebab-case slug derived from the issue title, so an issue traces
through `specs/log-linear-interp.md → designs/log-linear-interp.md → ...` end-to-end.

## How to Invoke the Team

- **End-to-end on a GitHub issue.** "Use `dal-orchestrator` to handle issue #57." The
  orchestrator fetches the issue, decomposes the work, and delegates to teammates.
- **A single specialist.** Address the role directly: "Use `dal-architect` to design the
  multi-curve refactor described in `.claude/specs/multi-curve.md`."
- **Adversarial review of an existing plan.** "Use `dal-critic` on the design at
  `.claude/designs/foo.md`."

## Conventions Each Agent Honors

- `.claude/rules/code-style.md` — naming, headers, includes, error handling, enums (Machinist)
- `.claude/rules/unit-test-style.md` — Google Test patterns, assertions, suite naming
- `.claude/rules/git-commit-pr.md` — branch naming, commit message format, PR template
- `.claude/methodology/*.md` — domain vocabulary; quant claims must match these docs

## Hand-off Etiquette

- One agent at a time per artifact. Don't fan out the same artifact to two agents in parallel.
- Self-contained prompts. The teammate agent doesn't see the parent conversation, so the
  invocation must include all paths, decisions, and acceptance criteria it needs.
- Verify before advancing. The orchestrator checks each artifact exists and has the expected
  shape (acceptance criteria, file map, verdict, etc.) before the next step starts.
- A `Block` verdict from the critic routes work back to the upstream author, not forward to
  the implementer.
