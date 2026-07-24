---
name: dal-agent-team
description: Coordinate the DAL specialist role set converted from `.claude/agents`. Use when the user asks to run the team, use a DAL agent by role, delegate through the DAL pipeline, pick the right DAL specialist, or reproduce the spec, API, critique, implement, test, review, docs workflow in Codex.
---

# DAL Agent Team

Use this skill as the Codex-native index for the DAL role team. The original Claude role
files remain under `.claude/agents/`; each Codex role has a custom-agent registration and a
same-name workflow skill.

## Role Map

| Role         | Custom agent                          | Workflow skill     | Use for                                   |
|--------------|---------------------------------------|--------------------|-------------------------------------------|
| Orchestrator | `.codex/agents/dal-orchestrator.toml` | `dal-orchestrator` | Planning and delegation across roles      |
| Spec writer  | `.codex/agents/dal-spec-writer.toml`  | `dal-spec-writer`  | Turning vague asks into testable specs    |
| API designer | `.codex/agents/dal-api-designer.toml` | `dal-api-designer` | Public C++/Python/Excel/example API shape |
| Critic       | `.codex/agents/dal-critic.toml`       | `dal-critic`       | Attacking specs, designs, and proposals   |
| Implementer  | `.codex/agents/dal-implementer.toml`  | `dal-implementer`  | TDD feature and bug-fix implementation    |
| Tester       | `.codex/agents/dal-tester.toml`       | `dal-tester`       | Test coverage and failing test repair     |
| Reviewer     | `.codex/agents/dal-reviewer.toml`     | `dal-reviewer`     | PR/code review and merge gate             |
| Performancer | `.codex/agents/dal-performancer.toml` | `dal-performancer` | Benchmark regression and coverage advice  |
| Simplifier   | `.codex/agents/dal-simplifier.toml`   | `dal-simplifier`   | Duplication and simplification sweeps     |
| Doc writer   | `.codex/agents/dal-doc-writer.toml`   | `dal-doc-writer`   | Docs and changelog reconciliation         |

## Codex Adaptation Rules

- Use the same role names as the Claude team when the user asks for them.
- Delegate with the role's custom-agent registration under `.codex/agents/*.toml` only when the user explicitly authorizes delegation, team execution, parallel agents, or subagents.
- Otherwise, emulate the selected role in the current Codex session by loading its same-name workflow skill before acting.
- Load `references/shared-rules.md` when a role needs shared style, test, docs, review, or artifact conventions.
- Write new durable role artifacts under `.codex/artifacts/`, not `.claude/`, unless the user explicitly asks to update Claude artifacts.
- Treat `.claude/agents/` as read-only source material.
- When delegating custom agents, give each a self-contained prompt, disjoint write scope, and the relevant `.codex/agents/<role>.toml` registration.

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
