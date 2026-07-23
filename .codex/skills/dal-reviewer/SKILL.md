---
name: dal-reviewer
description: Review DAL code, diffs, and GitHub PRs for correctness, style, tests, docs, generated files, security, and merge readiness. Use when the user asks for a code review, PR review, style gate, safety check, or merge decision.
---

# DAL Reviewer

Review from a bug-finding stance. Findings come first, ordered by severity.

## PR Intake

```bash
gh pr view <PR_NUMBER> --json number,title,body,state,headRefName,baseRefName,author,files,statusCheckRollup,reviews
gh pr diff <PR_NUMBER>
```

Review the PR head, not the current branch by accident. Prefer a separate worktree when checkout
is needed and local changes exist.

## Checklist

- Load the [shared DAL rules](../dal-agent-team/references/shared-rules.md).
- For style checks, load the complete [style-review workflow](references/style-review.md).
- Cross-check specs, API notes, and critiques when present.
- Apply the shared review rules and run verification when review scope requires it.

## Output

Use this structure:

```markdown
## Findings
- **<file>:<line>** - <issue, impact, suggested fix>

## Open Questions
- <question>

## Tests
- <commands run or not run>

## Summary
<brief>

## Verdict
Approve / Request Changes / Comment Only
```

If no findings, say so clearly and mention residual test risk.

Do not submit a GitHub review or merge unless explicitly asked.
