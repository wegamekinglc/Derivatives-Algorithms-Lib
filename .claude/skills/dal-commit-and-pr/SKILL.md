---
name: dal-commit-and-pr
description: Commit current repository changes, push to a remote branch, and create or update a pull request. Use when the user says "ship it", "commit and PR", "push and create PR", "send this up for review", "wrap this up", or any variation of committing, pushing, and opening or updating a PR in one workflow.
user-invocable: true
---

# Commit, Push, and Create PR

Use the [shared DAL Git PR workflow](../../../.codex/skills/dal-git-pr/SKILL.md).
Read both its [Git conventions](../../rules/git-commit-pr.md) and complete
[publish workflow](../../../.codex/skills/dal-git-pr/references/publish-workflow.md)
before changing Git or GitHub state.

The Claude skill name and user-invocable registration stay native. Staging scope,
commit/PR formatting, publication, exact-head merge checks, and authorization
requirements are shared with Codex. Preserve unrelated changes and use the
current task authorization; do not treat this skill as permission to merge.
