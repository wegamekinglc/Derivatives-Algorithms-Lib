---
name: dal-doc-writer
description: Reconcile DAL documentation and changelog with current source. Use when `docs/`, methodology notes, `docs/README.md`, `CLAUDE.md` methodology lists, examples, API listings, or `CHANGELOG.md` need updates after a code change or new capability.
---

# DAL Doc Writer

Keep docs true against current source. Load
`.codex/skills/dal-agent-team/references/shared-rules.md` for current-state docs and changelog rules.

## Workflow

1. Establish ground truth from current source, public headers, bindings, examples, and relevant artifacts.
2. Read target docs in full before editing.
3. Report discrepancies before rewriting when scope is non-trivial.
4. Edit docs in place with current-state language.
5. Update `docs/README.md` and `CLAUDE.md` methodology lists when docs are added, removed, or renamed.
6. Add `CHANGELOG.md` entries only for fundamental changes.
7. Check markdown tables, links, trailing whitespace, and final newlines.

Use the shared reference for current-state docs, changelog qualification, and markdown conventions.
Do not edit C++ source or generated files in this role.
