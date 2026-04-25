# Git Commit and PR Guide

## Branch Naming

- Feature branches: `feature/<short-description>` (e.g., `feature/log-linear-interp-and-refactor`)
- Bug fix branches: `fix/<short-description>`
- Base branch: `master`

## Commit Messages

- First line: imperative summary under 72 characters (e.g., "Add log-linear interpolation", "Fix include paths")
- Blank line, then body explaining **why** the change was made, not just what changed
- If the user wants an AI co-author trailer, use their preferred trailer; otherwise append `Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>`
- One logical change per commit -- don't mix unrelated fixes

**Good examples:**
```
Add log-linear interpolation and extract linear interp into separate files

- Add new LogLinear1_ interpolator that interpolates linearly on log(f)
- Extract Interp1Linear_ from interp.hpp into dedicated interplinear.hpp/cpp
- Update all dependent files to include interplinear.hpp directly
```

```
Fix include paths and ordering in interp module

Correct interploglinear.hpp include to use interp.hpp instead of interplinear.hpp,
and reorder includes in test files to follow project convention (gtest first).
```

**Bad examples:**
- `update` (too vague)
- `some code modification` (says nothing)
- `Fix bug and add feature and refactor` (too many things at once)

## Pull Requests

- Branch from `master`, PR back to `master`
- Title: short summary under 70 characters
- Body format:

```markdown
## Summary
- Bullet points describing the key changes

## Test plan
- [x] Run specific test filters to verify new/changed functionality
- [x] Full `bin/test_suite` to confirm no regressions
```

- All tests must pass before merging
- Keep PRs focused -- one feature or fix per PR when possible
