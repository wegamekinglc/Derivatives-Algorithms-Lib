---
name: dal-critic
description: Adversarially review DAL specs, API notes, designs, and proposals before implementation. Use to find hidden assumptions, missing edge cases, incorrect methodology, weak acceptance criteria, performance risk, compatibility problems, or quietly bad tradeoffs in plans, not already-implemented code.
---

# DAL Critic

Attack plans before they harden into code. Do not implement fixes while operating as critic.

## Read First

- Target spec, API note, design, or proposal.
- Relevant methodology docs.
- Closest existing analogue in source.
- Tests for the analogue.

## Axes

- Correctness: methodology, units, day-count, compounding, AAD differentiability.
- Hidden assumptions: sorted input, non-empty input, thread/tape/currency assumptions.
- Edge cases: empty, single element, boundaries, duplicate dates, zero/negative, NaN/Inf.
- Compatibility: callers, tests, serialization, bindings.
- Performance: complexity, allocation, hot loops, serialized paths.
- Surface: naming, argument order, defaults, error messages.
- Test plan: whether tests are real and fail against stubs.
- Scope: whether the smallest useful version is smaller than the proposal.

## Artifact Path

Write new Codex critiques to:

```text
.codex/artifacts/critiques/<feature-slug>.md
```

## Verdicts

Use one: `Block`, `Revise`, `Proceed with caveats`, `Looks fine`.

## Output Shape

```markdown
# <Feature Name> - Critique

## Target
- Spec:
- API note:

## Verdict
**Block / Revise / Proceed with caveats / Looks fine**

## Findings

### Blocking Issues
- **<title>** - <problem, evidence, impact>
  - **Suggested fix:** <concrete change>

### Significant Concerns
- **<title>** - <problem and fix>

### Minor / Style Notes
- <note>

## Counter-Proposals
<brief alternatives>

## Questions for the Author
- <question>
```
