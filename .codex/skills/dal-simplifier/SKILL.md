---
name: dal-simplifier
description: Find simplification opportunities in implemented DAL code. Use after implementation or on a named module/diff to detect duplicated logic, near-duplicate types, dead code, verbose constructs, duplicated test setup, and oversized source comments that belong in methodology docs.
---

# DAL Simplifier

Read implemented code and recommend simplifications. Default mode is report-only. Edit only when
the user explicitly asks for apply/fix mode.

## Read

- Full changed files or named module files.
- Closest existing analogue.
- Relevant tests.
- `.codex/skills/dal-agent-team/references/shared-rules.md` conventions for no duplication and comment style.

## Look For

- Duplicated functions, branches, switch cases, or setup blocks.
- Near-duplicate templated and non-templated types.
- Dead, unreachable, or redundant code.
- Work computed and discarded.
- Hand-rolled loops with clearer standard-library or existing-helper forms.
- Large explanatory comments that should move to `docs/methodology/`.

## Report Shape

```markdown
## Simplification Report: <target>

### Summary
<dominant pattern or clean result>

### Findings

#### 1. <short title>
- **Sites:** <files and symbols>
- **Pattern:** duplicated logic / near-duplicate type / dead code / verbose construct / oversized comment
- **Rule:** <project rule>
- **Why it matters:** <impact>
- **Recommended fix:** <helper/template/lambda/table/delete/doc migration>
- **Risk:** low / medium / high

### Not Findings
- <axis checked and clean>
```

Do not cite source line numbers in simplification reports; cite symbols and files.
