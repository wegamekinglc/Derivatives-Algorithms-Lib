---
name: dal-git-pr
description: Package DAL repository changes into commits and pull requests. Use when the user says to commit, push, open a PR, update a PR, ship it, send for review, or wrap up current work.
---

# DAL Git PR

## Overview

Use this skill to turn completed work into focused commits and a PR. Preserve unrelated
user changes and never stage dirty submodule contents or generated build artifacts.

Load both the shared [DAL git conventions](../dal-agent-team/references/git-commit-pr.md)
and the complete [publish workflow](references/publish-workflow.md) before changing Git or
GitHub state.

## Workflow

1. Inspect state:

```bash
git status --short
git diff --stat
git diff --staged --stat
git log --oneline -5
```

2. Determine the branch. Use an existing feature/fix branch when appropriate; create a new branch from `master` if currently on `master`.
3. Review the diff carefully. Stage only files that belong to this logical change. Prove the
   exact staged scope with `git diff --cached --name-status` and validate it with
   `git diff --cached --check`.
4. Commit with an imperative summary under 72 characters and a body explaining why.
5. Push the branch.
6. Create or update the PR with a concise title under 70 characters.
7. If merge is requested, inspect unresolved review threads, verify checks for the exact current
   head SHA, and merge only with `gh pr merge --match-head-commit <sha>` as detailed in the
   publish workflow.

## Commit Message

Use one logical change per commit:

```text
Add log-linear interpolation

Explain why the change is needed and what behavior it enables.
```

Do not append co-author trailers unless the user explicitly asks.

## PR Body

Use this shape:

```markdown
## Summary
- <key change>
- <key change>

## Test plan
- [x] <targeted test or build>
- [x] <full suite or rationale if not run>
```

Allowed PR title prefixes: `feat:`, `fix:`, `docs:`, `chore:`, `refactor:`, `test:`,
`style:`, `perf:`, `ci:`. Use draft PRs for unfinished work.

## Submodules

Stage submodule pointer updates only when intentional. Do not stage dirty submodule
working tree contents.
