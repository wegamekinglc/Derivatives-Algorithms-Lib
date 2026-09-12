DAL-199 prerequisite documentation handoff, 2026-09-13. Reconciled against
tester head `18ff785769581fb1ea0f0c978d46e5d43a4540ec`, production handoff
`b15e5340853d6d074c4fd9be969ac16141d36080`, and the approved API note
`.codex/artifacts/api-notes/dal-199-fixhistory-identity.md`. The containing
commit is the documentation handoff on `fix/dal-199-fixing-history-definition`.

## Documentation and CHANGELOG Decision

- Read both complete target documents, the API decision, implementation and
  independent testing reports, and the full production diff. Inspected
  `dal-cpp/dal/indice/fixings.hpp`, `dal-cpp/dal/indice/fixings.cpp`,
  `dal-cpp/dal/indice/index.hpp`, and `dal-cpp/dal/storage/globals.hpp`.
  Confirmed the public-facade, Python, and Excel trees
  have no delta; symbol search finds no direct history-wrapper references in
  those surfaces or the examples.
- `docs/methodology/index_parsing.md` now distinguishes the copied-map
  `Dal::IndexFixHistory_`, the process-lifetime const result of
  `FixHistory::Empty()`, the vector aggregate `Dal::FixHistory_` used by global
  storage, and the separate named storable `Dal::Fixings_` in fixing access
  environments. It records exact lookup and unchanged missing/quiet behavior,
  without adding value validation, FX inversion, or synchronization promises.
- CHANGELOG decision: a September 13 entry is required because direct core
  consumers face a breaking C++ source/ABI rename. It identifies map-wrapper
  type and nested `vals_t` migration, explicitly typed `Empty()` results, the
  preserved global vector name/layout, absence of a map compatibility alias,
  and a clean rebuild of DAL plus all dependent C++ objects/binaries including
  bindings and executables. Old conflicting objects cannot be mixed with the
  repaired build. The entry preserves unchanged lookup, facade/binding, and
  serialization contracts.
- No methodology page was added, removed, or renamed; `docs/README.md` and
  the `CLAUDE.md` methodology list need no change. No Python/Excel API guide,
  example, C++, test, generated, or configuration edit is needed in this role.
  Migration history is confined to CHANGELOG; methodology is current-state.

## Validation and Remaining Gates

- `python3 .github/scripts/check_docs.py`: passed across all repository
  Markdown, including local links/anchors, tables, trailing whitespace, final
  newlines, and metadata/workflow rules.
- `git diff --check` and `git diff --cached --check`: passed. Exact staged
  scope is the two published documents above and this evidence artifact.
- Independent tester evidence at the stated head is a clean native full
  build/install run with 1595/1595 tests passing, both header orders compiling,
  and freshly compiled actual-header cross-TU `-O0` ASan reproductions passing
  in both link orders. Full commands/configuration/limits are in `testing.md`
  alongside this file. This documentation pass did not rerun runtime tests or
  broaden those claims to full-suite sanitizers, Windows/XLL, or Python.
- No material documentation/source mismatch remains for this rename. The
  original F2 checkout was untouched; this prerequisite branch contains no F2
  preparation or debug repair. Independent final review, publication, and
  integration/revalidation with F2 remain with the orchestrator. No push or
  platform/GitHub write was performed by this role.
