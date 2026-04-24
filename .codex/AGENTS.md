# Codex Notes For This Repository

This directory contains Codex-facing copies of the reusable project guidance that also exists under `.claude`.

## How To Use These Files

- Use `rules/code-style.md` when editing or reviewing C++ source, public headers, examples, and tests.
- Use `rules/unit-test-style.md` when adding or changing Google Test coverage.
- Use `rules/git-commit-pr.md` before committing, pushing, or creating a pull request.
- Use `methodology/yield_curve.md` for curve construction, discount curve, calibration, or underdetermined optimization work.

## Local Workflow Skills

The folders under `skills/` use Codex-compatible `SKILL.md` frontmatter. If a user request matches one of these workflows, read the matching file and follow it.

- `skills/commit-and-pr/SKILL.md`: use for "ship it", "commit and PR", "push and create PR", "send this up for review", "wrap this up", or similar commit + push + PR requests.
- `skills/code-style-review/SKILL.md`: use for style review, lint-like checks, naming convention checks, or "review my changes" when the likely intent is coding style.
- `skills/unit-test-skill/SKILL.md`: use for running the repository build/test workflow or verifying the whole test suite.
