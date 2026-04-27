---
name: dal-commit-and-pr
description: Commit current repository changes, push to a remote branch, and create or update a pull request. Use when the user says "ship it", "commit and PR", "push and create PR", "send this up for review", "wrap this up", or any variation of committing, pushing, and opening or updating a PR in one workflow.
user-invocable: true
---

# Commit, Push, and Create PR

This skill packages up the current session's work into commits, pushes to a remote branch, and opens or updates a pull request. It follows the project's git conventions defined in `.claude/rules/git-commit-pr.md`.

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

### 3. Handle submodules

Check for submodule changes:

```bash
git diff externals/
```

There are two kinds of submodule changes — handle them differently:

- **Pointer update** (the diff shows a new `Subproject commit` hash without `-dirty`): This means the submodule was intentionally updated to a newer commit. Stage it with `git add externals/<name>` and include it in the commit that motivated the update.
- **Dirty submodule** (the diff shows the same hash with `-dirty`, or `git status` shows `modified content` / `untracked content`): This means files inside the submodule were modified locally but the submodule pointer itself hasn't changed. Do NOT stage these — they are local build artifacts or accidental edits. Skip them.

### 4. Stage and commit

- Review the diff carefully to understand what changed and why.
- Skip files that should not be committed: binaries (`*.xll`), dirty submodules (see step 3), generated cmake files, secrets (`.env`, credentials).
- Group related changes into logical commits — one commit per logical unit of work. If all changes are related, a single commit is fine.
- Write commit messages following project conventions:
  - Imperative summary under 72 characters
  - Body explaining the "why", not just the "what"
  - If the user wants an AI co-author trailer, use their preferred trailer; otherwise append `Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>`
- Use a multi-line commit message to preserve formatting:
  ```bash
  git commit -m "$(cat <<'EOF'
  Summary line here

  Body explaining why.

  Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

### 5. Push to remote

```bash
git push -u origin <branch-name>
```

If the branch already tracks a remote, a plain `git push` is sufficient.

### 6. Create or update the pull request

First check if a PR already exists for this branch:

```bash
gh pr view --json number,title,body 2>/dev/null
```

#### If no PR exists — create one

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

#### If a PR already exists — update its description

Read the existing PR body, then amend it to incorporate the new commit(s). The goal is a single coherent description covering all work on the branch, not an append-only changelog.

1. Read all commits on the branch (`git log --oneline master..HEAD`) and the existing PR body.
2. Rewrite the Summary section to cover all changes holistically. Don't just append bullet points — reorganize if needed so the description reads well as a whole.
3. Update the Test plan if the new changes affect different test suites.
4. Apply with:
   ```bash
   gh pr edit --body "$(cat <<'EOF'
   ## Summary
   - <updated bullet points covering ALL changes on the branch>

   ## Test plan
   - [ ] Updated test plan items
   EOF
   )"
   ```
5. Optionally update the title if it no longer reflects the full scope:
   ```bash
   gh pr edit --title "<new title under 70 chars>"
   ```

### 7. Report back

Print the PR URL so the user can review it.
