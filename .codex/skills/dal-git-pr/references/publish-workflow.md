# DAL Publish Workflow Reference

This reference packages the current session's work into commits, pushes to a remote branch,
and opens or updates a pull request. Follow the
[Codex-owned DAL git conventions](../../../references/git-commit-pr.md).

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
and head, then define paginated audits for review threads and exact-head check runs:

```bash
repo_slug=$(gh repo view --json nameWithOwner --jq .nameWithOwner)
owner=${repo_slug%%/*}
repo=${repo_slug#*/}
pr_number=$(gh pr view --json number --jq .number)
head_sha=$(gh pr view "$pr_number" --json headRefOid --jq .headRefOid)

audit_review_threads() {
  gh api graphql --paginate \
    -F owner="$owner" -F repo="$repo" -F number="$pr_number" \
    -f query='
      query($owner:String!,$repo:String!,$number:Int!,$endCursor:String) {
        repository(owner:$owner,name:$repo) {
          pullRequest(number:$number) {
            reviewDecision
            reviewThreads(first:100, after:$endCursor) {
              nodes {
                isResolved
                isOutdated
                comments(first:100) {
                  nodes { path line body author { login } }
                }
              }
              pageInfo { hasNextPage endCursor }
            }
          }
        }
      }' \
    --jq '.data.repository.pullRequest.reviewThreads.nodes[]'
}

audit_check_runs() {
  gh api --paginate \
    "repos/$repo_slug/commits/$1/check-runs?per_page=100" \
    --jq '.check_runs[]'
}

require_same_head() {
  current_head=$(gh pr view "$pr_number" --json headRefOid --jq .headRefOid)
  if [ "$current_head" != "$head_sha" ]; then
    echo "PR head changed from $head_sha to $current_head; restart all completion gates." >&2
    return 1
  fi
}
```

The GraphQL query accepts `$endCursor`, requests
`reviewThreads(first:100, after:$endCursor)`, and returns
`pageInfo { hasNextPage endCursor }`; `gh api graphql --paginate` therefore retrieves every
thread page instead of silently stopping at 100.

Run the initial audit against the captured head:

```bash
gate_dir=$(mktemp -d)
audit_review_threads > "$gate_dir/review-threads-initial.jsonl" || exit 1
audit_check_runs "$head_sha" > "$gate_dir/check-runs-initial.jsonl" || exit 1
gh pr view "$pr_number" \
  --json headRefOid,reviewDecision,mergeable,mergeStateStatus,statusCheckRollup \
  > "$gate_dir/pr-state-initial.json" || exit 1
jq -e . "$gate_dir/pr-state-initial.json" > /dev/null || exit 1

jq -r '
  select(.isResolved == false)
  | {isOutdated, comments: [.comments.nodes[] | {path, line, body, author: .author.login}]}
' "$gate_dir/review-threads-initial.jsonl" || exit 1

jq -r '
  select(.status != "completed" or .conclusion != "success")
  | [.name, .status, .conclusion, .html_url] | @tsv
' "$gate_dir/check-runs-initial.jsonl" || exit 1
```

Inspect every unresolved thread returned across all pages, including outdated threads, and
require that no actionable thread remains. Inspect every check-run page for the exact captured
SHA and require all branch-policy-required checks to be completed and successful. Also require
the PR's review decision, mergeability, merge-state status, and rollup to permit merge. Each
audit writes one JSON object per line; an empty review-thread file is a valid zero-thread result.
An empty check-run file is also valid JSONL input, but it must be reconciled with branch policy
and `statusCheckRollup` before deciding that no checks are required. Plain `jq` exits cleanly for
an empty input file. The explicit `|| exit 1` guards distinguish that legitimate empty success
from a failed capture, truncated JSON, malformed JSONL, or filter error; none may proceed to merge.

Immediately before merge, repeat the full paginated audits and PR-state capture. A head change at
any point invalidates every prior result and stops the merge:

```bash
require_same_head || exit 1

audit_review_threads > "$gate_dir/review-threads-final.jsonl" || exit 1
jq -r '
  select(.isResolved == false)
  | {isOutdated, comments: [.comments.nodes[] | {path, line, body, author: .author.login}]}
' "$gate_dir/review-threads-final.jsonl" || exit 1

require_same_head || exit 1
audit_check_runs "$head_sha" > "$gate_dir/check-runs-final.jsonl" || exit 1
jq -r '
  select(.status != "completed" or .conclusion != "success")
  | [.name, .status, .conclusion, .html_url] | @tsv
' "$gate_dir/check-runs-final.jsonl" || exit 1

require_same_head || exit 1
gh pr view "$pr_number" \
  --json headRefOid,reviewDecision,mergeable,mergeStateStatus,statusCheckRollup \
  > "$gate_dir/pr-state-final.json" || exit 1
test "$(jq -er .headRefOid "$gate_dir/pr-state-final.json")" = "$head_sha" || exit 1
```

Reinspect the final unresolved-thread output, exact-head check-run output, `reviewDecision`,
`mergeable`, `mergeStateStatus`, and `statusCheckRollup`. If the head changed, start again by
capturing the new `head_sha` and rerunning both initial audits. Only after the repeated final
audits are clear, perform one last head check and use the merge guard:

```bash
require_same_head || exit 1
gh pr merge "$pr_number" --squash --match-head-commit "$head_sha"
```

### 8. Report back

Print the PR URL so the user can review it.
