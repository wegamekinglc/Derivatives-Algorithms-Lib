# DAL Publish Workflow Reference

This reference packages the current session's work into commits, pushes to a remote branch,
and opens or updates a pull request. Follow the
[Codex-owned DAL git conventions](../../dal-agent-team/references/git-commit-pr.md).

## Contents

- [Assess repository state](#1-assess-the-current-state)
- [Select or create a branch](#2-determine-the-branch)
- [Handle read-only Git metadata](#read-only-git-metadata)
- [Handle submodules](#3-handle-submodules)
- [Stage and commit](#4-stage-and-commit)
- [Push](#5-push-to-remote)
- [Create or update the pull request](#6-create-or-update-the-pull-request)
- [Completion gates](#7-completion-gates)
- [Report](#8-report-back)

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

#### Read-only Git metadata

If the workspace `.git` directory is read-only, leave it untouched and publish from a writable
temporary clone:

```bash
repo_slug=$(gh repo view --json nameWithOwner --jq .nameWithOwner)
publish_root=$(mktemp -d)
gh repo clone "$repo_slug" "$publish_root/repo"
```

Transfer only the intended working-tree patch into the clone, create or check out the target
branch there, and repeat the status, diff, staging, and validation checks in the clone. Do not
copy build outputs, secrets, dirty submodule contents, or unrelated user changes.

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
  - Do not append any co-author trailer
- Use a multi-line commit message to preserve formatting:
  ```bash
  git commit -m "$(cat <<'EOF'
  Summary line here

  Body explaining why.
  EOF
  )"
  ```
- Prove the exact staged scope and patch integrity immediately before committing:
  ```bash
  git diff --cached --name-status
  git diff --cached --check
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

#### PR title conventions

- Keep it under 70 characters.
- Use imperative mood (e.g., "feat: add log-linear interpolation" not "feat: adds log-linear interpolation").
- Start with a category prefix — this is mandatory. Choose from: `feat:`, `fix:`, `docs:`, `chore:`, `refactor:`, `test:`, `style:`, `perf:`, `ci:`.
- Describe the change at a high level, not implementation details.
- If the PR is still in progress, prefix with `WIP:` (before the category prefix) or open it as a draft.

#### If no PR exists — create one

Use `gh pr create` with the project's PR template. Analyze all commits on the branch (not just the latest) to write the summary.

```bash
gh pr create --title "<short title under 70 chars>" --body "$(cat <<'EOF'
## Summary
- <bullet points covering all key changes>

## Test plan
- [ ] Run `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=<RelevantSuite>.*` to verify changes
- [ ] Full `./build/Release-linux/dal-cpp/dal_cpp_tests` to confirm no regressions
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

### 7. Completion gates

When the user asks to merge, do not merge from an earlier green snapshot. Capture the current PR
and head, inspect thread-level review state, query checks for that exact SHA, and guard the merge
against a later push:

```bash
repo_slug=$(gh repo view --json nameWithOwner --jq .nameWithOwner)
owner=${repo_slug%%/*}
repo=${repo_slug#*/}
pr_number=$(gh pr view --json number --jq .number)
head_sha=$(gh pr view "$pr_number" --json headRefOid --jq .headRefOid)
```

Inspect every review thread and require all actionable threads to be resolved:

```bash
gh api graphql \
  -F owner="$owner" -F repo="$repo" -F number="$pr_number" \
  -f query='query($owner:String!,$repo:String!,$number:Int!){repository(owner:$owner,name:$repo){pullRequest(number:$number){reviewDecision reviewThreads(first:100){nodes{isResolved isOutdated comments(first:100){nodes{path line body author{login}}}}}}}}'
```

Query check runs for the captured head SHA and verify required runs have completed successfully:

```bash
gh api "repos/$repo_slug/commits/$head_sha/check-runs?per_page=100"
```

Immediately before merge, confirm `headRefOid` is still `"$head_sha"`, then use the head guard:

```bash
gh pr view "$pr_number" --json headRefOid,reviewDecision,statusCheckRollup
gh pr merge "$pr_number" --squash --match-head-commit "$head_sha"
```

### 8. Report back

Print the PR URL so the user can review it.
