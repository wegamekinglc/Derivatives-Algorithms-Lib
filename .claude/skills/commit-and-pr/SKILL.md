---
name: commit-and-pr
description: Commit all current changes, push to remote, and create a pull request. Use this skill whenever the user says "ship it", "commit and PR", "push and create PR", "send this up for review", "wrap this up", or any variation of committing + pushing + opening a PR in one go.
user-invocable: true
---

# Commit, Push, and Create PR

This skill packages up the current session's work into commits, pushes to a remote branch, and opens a pull request — all in one shot. It follows the project's git conventions defined in `.claude/rules/git-commit-pr.md`.

## Steps

### 1. Assess the current state

Run these in parallel to understand what needs to be committed:

```bash
git status
git diff --stat
git diff --staged --stat
git log --oneline -5
```

If there are no changes (staged or unstaged), tell the user and stop.

### 2. Determine the branch

- If already on a feature/fix branch, use it.
- If on `master`, create a new branch. Ask the user for a name, or suggest one based on the changes (e.g., `feature/add-interp-serialization-tests`).

### 3. Stage and commit

- Review the diff carefully to understand what changed and why.
- Skip files that should not be committed: binaries (`*.xll`), dirty submodules (`externals/`), generated cmake files, secrets (`.env`, credentials).
- Group related changes into logical commits — one commit per logical unit of work. If all changes are related, a single commit is fine.
- Write commit messages following project conventions:
  - Imperative summary under 72 characters
  - Body explaining the "why", not just the "what"
  - Append `Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>`
- Use a HEREDOC for the commit message to preserve formatting:
  ```bash
  git commit -m "$(cat <<'EOF'
  Summary line here

  Body explaining why.

  Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

### 4. Push to remote

```bash
git push -u origin <branch-name>
```

If the branch already tracks a remote, a plain `git push` is sufficient.

### 5. Create the pull request

Use `gh pr create` with the project's PR template. Analyze all commits on the branch (not just the latest) to write the summary.

```bash
gh pr create --title "<short title under 70 chars>" --body "$(cat <<'EOF'
## Summary
- <bullet points covering all key changes>

## Test plan
- [ ] Run `bin/test_suite --gtest_filter=<RelevantSuite>.*` to verify changes
- [ ] Full `bin/test_suite` to confirm no regressions
EOF
)"
```

The base branch is always `master`.

### 6. Report back

Print the PR URL so the user can review it.
