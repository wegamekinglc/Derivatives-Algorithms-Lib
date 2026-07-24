# Codex Guidance

This directory is the repository-owned home for Codex specialists, reusable conventions,
non-agent workflows, and active work records.

## Surfaces

- `.codex/agents/` registers ten named DAL specialists and contains their complete role contracts.
- `.codex/references/` contains shared C++, test, review, performance, and Git conventions.
- `.codex/skills/` contains only reusable workflows without dedicated agents: `dal-git-pr` and `dal-web`.
- `.codex/artifacts/` contains only work products that still control active work.

`CLAUDE.md` remains canonical only for shared build/test commands and architecture.

## DAL Agent Team

Use `dal-orchestrator` for end-to-end routing. Use the matching named agent for focused
specification, API, critique, implementation, testing, review, performance, simplification,
or documentation work.

Delegate only when the user authorizes agents, team execution, parallel work, or subagents.
Without that authority, read the relevant `.codex/agents/{role}.toml` and follow its contract
in the current session.

## References

Agent contracts link only the references needed for their role. Git/PR and web references that
belong to one surviving workflow remain inside that skill; cross-role conventions live directly
under `.codex/references/`.

## Active Artifacts

Create durable artifacts only when they control active work. Remove completed artifacts after
their current-state outcome is documented and Git history preserves implementation and review
history.

The retained `designs/api-shape-dedup.md` is active because it is design-only and still awaits
approval.

## Preserved Claude Sources

Tracked `.claude/**` and `CLAUDE.md` are preserved unless explicitly requested. Do not migrate
`.claude/settings.local.json` or `.claude/worktrees/`.

## Validation

Validate all agent TOMLs, both remaining skills and their YAML metadata, direct local Markdown
links, exact reference and artifact sets, protected Claude paths, and `git diff --check`.
