# Codex Guidance

This directory is the repository-owned home for Codex routing and durable work records.

## Surfaces

- `.codex/agents/` registers native named DAL specialists for authorized delegation.
- `.codex/skills/` owns reusable workflows and their output contracts.
- `.codex/skills/*/references/` contains the shared coding, testing, web, git, and team
  conventions used by those workflows.
- `.codex/artifacts/` owns durable outputs.

`CLAUDE.md` remains canonical only for shared build/test commands and architecture.

## DAL Agent Team

Use `dal-agent-team` to choose or coordinate DAL roles. Each named specialist registered in
`.codex/agents/` has a same-name workflow under `.codex/skills/`. Delegate only when the user
authorizes team execution, parallel work, or subagents; otherwise load the matching workflow in
the current session.

## Workflow Consolidation

Skills are the reusable task entry points. The role workflows cover specification, API design,
critique, implementation, tests, review, performance, simplification, documentation, web work,
and Git/PR packaging without duplicating legacy workflow bodies. Shared conventions live in the
Codex references they link.

## Artifact Layout

Write durable work to `.codex/artifacts/` using its established directories: `specs/`,
`designs/`, `api-notes/`, `critiques/`, `reviews/`, `plans/`, `perf/`, and
`simplifications/`. Use current repository paths for new artifacts.

## Preserved Claude Sources

Tracked `.claude/**` files and `CLAUDE.md` are preserved read-only unless explicitly requested.
They are source material, not an operational dependency for Codex workflows. Do not migrate
`.claude/settings.local.json` or `.claude/worktrees/`; both are explicitly excluded from this
migration.

## Validation

Validate custom-agent TOML registrations against their same-name skills, run the skill validator
over each `dal-*` skill, check direct local Markdown links, and confirm no operational Codex
guidance depends on `.claude/rules/` or `.claude/skills/`. Before publishing, confirm the Claude
sources have no diff and run `git diff --check`.
