---
name: dal-agent-team
description: Coordinate the DAL specialist role set converted from `.claude/agents`. Use when the user asks to run the team, use a DAL agent by role, delegate through the DAL pipeline, pick the right DAL specialist, or reproduce the spec, API, critique, implement, test, review, docs workflow in Codex.
---

# DAL Agent Team

Use this skill as the Codex-native index for the DAL role team. The original Claude role
files remain under `.claude/agents/`; Codex roles live as same-name skills under
`.codex/skills/`.

## Role Map

| Role         | Codex skill        | Use for                                      |
|--------------|--------------------|----------------------------------------------|
| Orchestrator | `dal-orchestrator` | Planning and delegation across roles          |
| Spec writer  | `dal-spec-writer`  | Turning vague asks into testable specs        |
| API designer | `dal-api-designer` | Public C++/Python/Excel/example API shape     |
| Critic       | `dal-critic`       | Attacking specs, designs, and proposals       |
| Implementer  | `dal-implementer`  | TDD feature and bug-fix implementation        |
| Tester       | `dal-tester`       | Test coverage and failing test repair         |
| Reviewer     | `dal-reviewer`     | PR/code review and merge gate                 |
| Performancer | `dal-performancer` | Benchmark regression and coverage advice      |
| Simplifier   | `dal-simplifier`   | Duplication and simplification sweeps         |
| Doc writer   | `dal-doc-writer`   | Docs and changelog reconciliation             |

## Codex Adaptation Rules

- Use the same role names as the Claude team when the user asks for them.
- Load the role's same-name skill before acting.
- Load `references/shared-rules.md` when a role needs shared style, test, docs, review, or artifact conventions.
- Write new durable role artifacts under `.codex/artifacts/`, not `.claude/`, unless the user explicitly asks to update Claude artifacts.
- Treat `.claude/agents/` as read-only source material.
- Spawn subagents only when the user explicitly asks for delegation, team execution, parallel agents, or subagents. Otherwise, emulate the selected role in the current Codex session.
- When spawning subagents, give each a self-contained prompt, disjoint write scope, and the relevant `.codex/skills/<role>` path.

## Default Pipeline

Use this order when the user asks to run the team end-to-end:

```text
spec-writer -> api-designer -> critic -> implementer -> tester -> reviewer -> doc-writer
```

Skip `api-designer` when no public API, binding, or example surface changes. Skip `doc-writer`
only for pure test additions or behavior-preserving refactors. Keep `reviewer` as the in-band
gate. Run `performancer` and `simplifier` out of band when requested.

## References

- `references/shared-rules.md`: single shared convention reference for all DAL role skills.
- `references/team-map.md`: concise source-to-Codex mapping for the converted roles.
