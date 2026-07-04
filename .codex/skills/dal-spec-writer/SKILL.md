---
name: dal-spec-writer
description: Convert fuzzy DAL requests, thin GitHub issues, performance asks, or one-line feature ideas into precise, testable requirement specifications. Use when scope, inputs, outputs, acceptance criteria, edge cases, AAD behavior, performance targets, or compatibility constraints are unclear before implementation.
---

# DAL Spec Writer

Turn vague asks into a testable requirement spec. Do not write implementation code.

## Workflow

1. Gather source material: user prompt, GitHub issue body/comments when provided, relevant headers, existing tests, methodology docs, and existing artifacts.
2. Ask only targeted questions when the answer cannot be inferred safely.
3. Define goals, non-goals, functional requirements, non-functional requirements, inputs/outputs, and acceptance criteria.
4. Write a durable spec only when requested or when the team workflow needs one.

## Artifact Path

Write new Codex specs to:

```text
.codex/artifacts/specs/<feature-slug>.md
```

Do not write `.claude/specs/` unless explicitly asked.

## Spec Template

```markdown
# <Feature Name> - Specification

## Source
- Issue: #<N> or user request on <date>
- Related methodology: <docs/methodology/*.md>

## Problem Statement
<2-4 sentences>

## Goals
- <testable outcome>

## Non-Goals
- <explicit out-of-scope item>

## Functional Requirements
- **FR1** - <verifiable behavior>

## Non-Functional Requirements
- **Performance** - <target, workload, measurement>
- **Differentiability** - <AAD requirements, if any>
- **Compatibility** - <what must not break>

## Inputs and Outputs
| Name | Type | Units | Range / Constraints |
|------|------|-------|---------------------|

## Acceptance Criteria
- [ ] Given <state>, when <action>, then <observable result>
- [ ] Relevant tests pass
- [ ] Documentation updated where applicable

## Open Questions
- <unresolved item>
```

Acceptance criteria must be executable as tests, build checks, or measurable observations.
