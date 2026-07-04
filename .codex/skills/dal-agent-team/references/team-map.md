# DAL Agent Team Mapping

Original source: `.claude/agents/`.

| Claude file                         | Codex skill                     | Notes                                      |
|-------------------------------------|---------------------------------|--------------------------------------------|
| `.claude/agents/dal-orchestrator.md` | `.codex/skills/dal-orchestrator` | Uses Codex subagents only when authorized. |
| `.claude/agents/dal-spec-writer.md`  | `.codex/skills/dal-spec-writer`  | Writes Codex specs by default.            |
| `.claude/agents/dal-api-designer.md` | `.codex/skills/dal-api-designer` | Writes Codex API notes by default.        |
| `.claude/agents/dal-critic.md`       | `.codex/skills/dal-critic`       | Writes Codex critiques by default.        |
| `.claude/agents/dal-implementer.md`  | `.codex/skills/dal-implementer`  | Uses normal Codex edit and test workflow. |
| `.claude/agents/dal-tester.md`       | `.codex/skills/dal-tester`       | Uses shared test conventions.             |
| `.claude/agents/dal-reviewer.md`     | `.codex/skills/dal-reviewer`     | Uses review-first answer shape.           |
| `.claude/agents/dal-performancer.md` | `.codex/skills/dal-performancer` | Best-of-N benchmark gate.                 |
| `.claude/agents/dal-simplifier.md`   | `.codex/skills/dal-simplifier`   | Read-only unless apply mode is requested. |
| `.claude/agents/dal-doc-writer.md`   | `.codex/skills/dal-doc-writer`   | Uses current-state docs rule.             |

The Claude-specific tool names `Agent`, `EnterWorktree`, `TaskCreate`, and `SendMessage`
do not appear in the Codex role files. Codex uses available tools directly and follows the
session's subagent policy.

Shared role conventions live in `.codex/skills/dal-agent-team/references/shared-rules.md`.
