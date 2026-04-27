# Codex Notes For This Repository

This directory contains Codex-facing copies of the reusable project guidance that also exists under `.claude`.

Keep the mirrored guidance semantically aligned across both trees. The only intentional differences should be tool-specific metadata or defaults, such
as assistant-specific co-author trailers and Claude-only local permission settings.

Use the root `CLAUDE.md` and `AGENTS.md` as the high-level repo overview. If any guidance conflicts with the live codebase, trust the current build
scripts, `CMakeLists.txt`, `CMakePresets.json`, and source tree.

## How To Use These Files

- Use `rules/code-style.md` when editing or reviewing C++ source, public headers, examples, and tests.
- Use `rules/unit-test-style.md` when adding or changing Google Test coverage.
- Use `rules/git-commit-pr.md` before committing, pushing, or creating a pull request.
- Use `methodology/aad.md` for automatic differentiation, expression templates, tape management, or adjoint propagation work.
- Use `methodology/yield_curve.md` for curve construction, discount curve, calibration, or underdetermined optimization work.
- Use `methodology/underdetermined_search.md` when working on the solver itself, weight construction, Jacobian handling, or convergence behavior.
- Use `agents/dal-dev-workflow.md` when implementing a feature, developing a new module, or executing a requirement specification end-to-end.

## Local Workflow Skills

The folders under `skills/` use Codex-compatible `SKILL.md` frontmatter. If a user request matches one of these workflows, read the matching file
and follow it.

- `skills/dal-commit-and-pr/SKILL.md`: use for "ship it", "commit and PR", "push and create PR", "send this up for review", "wrap this up", or
  similar commit + push + PR requests.
- `skills/dal-code-style-review/SKILL.md`: use for style review, lint-like checks, naming convention checks, or "review my changes" when the likely
  intent is coding style.
- `skills/dal-dev-workflow/SKILL.md`: use for feature implementation, new modules, or requirement specifications that need design, implementation,
  tests, and verification.
- `skills/dal-unit-test-write/SKILL.md`: use for writing Google Test coverage for new or existing C++ code.
- `skills/dal-unit-test-skill/SKILL.md`: use for running the repository build/test workflow or verifying the whole test suite.
