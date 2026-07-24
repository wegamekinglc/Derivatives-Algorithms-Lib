# DAL Agent Team Mapping

Original source: `.claude/agents/`.

| Claude file                          | Codex custom agent                    | Codex workflow skill              | Notes                                      |
|--------------------------------------|---------------------------------------|-----------------------------------|--------------------------------------------|
| `.claude/agents/dal-orchestrator.md` | `.codex/agents/dal-orchestrator.toml` | `.codex/skills/dal-orchestrator/` | Uses Codex subagents only when authorized. |
| `.claude/agents/dal-spec-writer.md`  | `.codex/agents/dal-spec-writer.toml`  | `.codex/skills/dal-spec-writer/`  | Writes Codex specs by default.             |
| `.claude/agents/dal-api-designer.md` | `.codex/agents/dal-api-designer.toml` | `.codex/skills/dal-api-designer/` | Writes Codex API notes by default.         |
| `.claude/agents/dal-critic.md`       | `.codex/agents/dal-critic.toml`       | `.codex/skills/dal-critic/`       | Writes Codex critiques by default.         |
| `.claude/agents/dal-implementer.md`  | `.codex/agents/dal-implementer.toml`  | `.codex/skills/dal-implementer/`  | Uses normal Codex edit and test workflow.  |
| `.claude/agents/dal-tester.md`       | `.codex/agents/dal-tester.toml`       | `.codex/skills/dal-tester/`       | Uses shared test conventions.              |
| `.claude/agents/dal-reviewer.md`     | `.codex/agents/dal-reviewer.toml`     | `.codex/skills/dal-reviewer/`     | Uses review-first answer shape.            |
| `.claude/agents/dal-performancer.md` | `.codex/agents/dal-performancer.toml` | `.codex/skills/dal-performancer/` | Best-of-N benchmark gate.                  |
| `.claude/agents/dal-simplifier.md`   | `.codex/agents/dal-simplifier.toml`   | `.codex/skills/dal-simplifier/`   | Read-only unless apply mode is requested.  |
| `.claude/agents/dal-doc-writer.md`   | `.codex/agents/dal-doc-writer.toml`   | `.codex/skills/dal-doc-writer/`   | Uses current-state docs rule.              |

The Claude-specific tool names `Agent`, `EnterWorktree`, `TaskCreate`, and `SendMessage`
do not appear in the Codex role files. Codex uses available tools directly and follows the
session's subagent policy.

Shared role conventions live in `.codex/skills/dal-agent-team/references/shared-rules.md`.
