# Repository Documentation Reconciliation - Specification

## Source

- User request: repository-wide documentation review and update, approved 2026-07-15.
- Baseline: merged `master` at `1589089bdf10df352ce5cf9cde963fd6b51a4f95`.
- Published-document set: `.github/scripts/check_docs.py` (`DOCS`, 33 Markdown files).
- Prior audit: `.codex/artifacts/reviews/codebase-documentation-audit.md` at commit `7b959542`.
- Latest capability references: `docs/methodology/xccy_calibration.md`,
  `docs/methodology/yield_curve_jacobian.md`, and public C++/Python/Excel surfaces.

## Problem Statement

DAL's automated documentation checker passes, but it verifies structural integrity rather
than full agreement with the current implementation. Since the prior repository audit,
substantial numerical, runtime, packaging, curve, and XCCY work has landed. Most detailed
documentation is current, but several public claims, architecture flows, API spellings,
surface-availability statements, example listings, and indexes are incomplete or stale.

The repository needs a source-backed audit of every published Markdown file, followed by
focused edits that describe the current library without turning overview pages into copies
of the methodology documents.

## Goals

- Audit all 33 Markdown files selected by `.github/scripts/check_docs.py` against the
  current implementation.
- Revalidate every still-relevant finding in the 2026-07-10 codebase documentation audit.
- Correct inaccurate API names, quantitative claims, runtime ownership descriptions,
  architecture flows, build commands, and cross-surface availability statements.
- Make major current capabilities discoverable from the root and documentation indexes.
- Record one auditable disposition for every published document: changed, verified current,
  or intentionally unchanged with evidence.
- Publish the reconciliation as a documentation-only follow-up pull request from `master`.

## Non-Goals

- No C++, Python, Excel, web, CMake, workflow, or generated-file behavior changes.
- No attempt to add the missing Excel worksheet getter for the joint XCCY effective inverse;
  document the current surface accurately instead.
- No hand edits under `dal-cpp/dal/auto/` or `dal-excel/auto/`.
- No edits to `CLAUDE.md`, `.claude/`, `AGENTS.md`, third-party documentation, build output,
  or Codex skill instructions.
- No speculative API promises, migration roadmap, or implementation history in current-state
  documentation. Historical context remains in `CHANGELOG.md`.
- No wholesale prose rewrite when a document is already accurate.

## Functional Requirements

- **FR1 - Complete published-document inventory:** Derive the audit set from the `DOCS`
  expression in `.github/scripts/check_docs.py`, not from an independently maintained list.
  The review artifact shall contain exactly one disposition for each selected document.
- **FR2 - Source-backed ground truth:** Validate claims against the closest authoritative
  source: public headers for C++, pybind11 and `dal/api.py` for Python, handwritten plus
  generated registrations for Excel, FastAPI/React source for web, CMake/presets/scripts for
  build instructions, and tests/examples/benchmarks for observable workflows.
- **FR3 - Prior-audit reconciliation:** Recheck the findings in
  `.codex/artifacts/reviews/codebase-documentation-audit.md` against current `master`. Current
  docs shall not repeat limitations that have been fixed or omit material constraints that
  remain.
- **FR4 - Root and index discoverability:** Keep `README.md` concise while adding links for
  major current capabilities that are otherwise absent, including cross-currency pricing and
  calibration. Correct the `docs/README.md` heading hierarchy and remove its obsolete claim
  that analytic Jacobians are exclusive to `LOG_DISCOUNT`.
- **FR5 - Architecture accuracy:** `docs/architecture.md` shall describe immutable
  operation-level `MarketFixingSnapshot_` ownership separately from the mutable process-wide
  fixing store. Its calibration flow shall distinguish generic single/staged calibration,
  staged XCCY basis calibration, and simultaneous domestic/foreign/basis XCCY calibration.
- **FR6 - Exact API spelling and availability:** Developer-facing identifiers shall match
  compilable or callable surface names. In particular, C++ notional-mode constants shall use
  `XccyNotionalMode_::Value_::*`. Documentation shall distinguish core/public staged XCCY
  options and matrices from the narrower Python and Excel staged result surfaces.
- **FR7 - XCCY fixing contract:** The XCCY methodology shall document canonical
  `FX[foreign/domestic]` names, reciprocal reverse-FX lookup, bidirectional reciprocity within
  the implemented tolerance, positive finite snapshot observations, duplicate behavior, and
  the unsettled-cashflow dependency closure for historical requests.
- **FR8 - Jacobian and inverse semantics:** The yield-curve Jacobian and XCCY methodology
  shall state joint matrix placement, dimensions, solved-state construction, exact/bumped/
  approximate population rules, and the existing `tolerance_` scaling convention without
  implying unavailable binding accessors.
- **FR9 - Examples and performance navigation:** Current documentation shall list all three
  registered C++ XCCY examples and the installed-surface Python joint example. It shall
  describe `xccy_perf` as a 24-row execution-smoke surface, including its started-MTM basket
  and reset-aware calibration coverage, without presenting it as a paired regression gate.
- **FR10 - Component README accuracy:** Reconcile the core, public, Python, Excel, and web
  READMEs with their actual current surfaces. The Python testing section shall include curve
  and XCCY coverage. The Excel docs shall explicitly state which joint matrices are and are not
  worksheet-visible.
- **FR11 - Changelog separation:** `CHANGELOG.md` may update its current-capability baseline
  and retain qualifying dated history. Current-state documents shall not contain delivery
  chronology, superseded plans, or historical implementation narrative.
- **FR12 - Verified no-change dispositions:** A document that needs no edit shall remain
  untouched and be marked verified-current in the audit artifact with its inspected source
  authorities. Passing `check_docs.py` alone is not sufficient evidence.

## Non-Functional Requirements

- **Accuracy:** Every changed technical claim must have direct evidence in current source,
  tests, examples, generated registration, or build configuration.
- **Compatibility:** Documentation-only changes must not alter public or internal behavior.
- **Current-state style:** Methodology owns mathematical contracts and invariants; overview
  pages summarize and link instead of duplicating detailed prose.
- **Markdown:** Preserve GitHub-renderable math, valid relative links and anchors, aligned
  tables, no trailing whitespace, and final newlines.
- **Scope control:** The final tracked diff may contain published Markdown and Codex audit/
  planning artifacts only. Generated and production files must remain unchanged.
- **Reviewability:** Prefer small thematic commits so API corrections, architecture/
  methodology reconciliation, and navigation updates can be reviewed independently.

## Inputs and Outputs

| Input                  | Authority                                                              | Output                                                                    |
|------------------------|------------------------------------------------------------------------|---------------------------------------------------------------------------|
| Published Markdown set | `.github/scripts/check_docs.py::DOCS`                                   | One 33-row disposition matrix                                             |
| C++ surface            | Core/public headers and generated enums                                | Exact types, constants, ownership, and solver contracts                   |
| Python surface         | `dal-python/src/bindings/`, `dal-python/src/dal/`, tests, examples      | Callable names, result properties, and supported workflows                |
| Excel surface          | `dal-excel/src/`, interface markup, generated help, smoke tests         | Worksheet functions, settings, selectors, and documented limitations     |
| Build/runtime surface  | CMake, presets, scripts, workflows, web source                          | Current setup, ownership, execution, and CI descriptions                  |
| Numerical behavior     | Methodology implementation plus independent tests                      | Correct formulas, matrix semantics, tolerances, and applicability         |

## Acceptance Criteria

- [ ] The audit artifact contains exactly one evidence-backed disposition for each of the 33
      Markdown files selected by `.github/scripts/check_docs.py`.
- [ ] Every still-relevant finding from the prior codebase documentation audit is either
      corrected in current docs or recorded as already resolved with current evidence.
- [ ] The confirmed root/index, architecture, enum-spelling, XCCY fixing, joint inverse,
      example, benchmark, Python testing, Excel availability, and changelog gaps are resolved.
- [ ] Root and component overview pages remain concise and link to methodology/API documents
      for detailed contracts.
- [ ] `CLAUDE.md`, `.claude/`, generated files, third-party docs, production source, build
      configuration, and workflows have no tracked changes.
- [ ] `python3 .github/scripts/check_docs.py` passes.
- [ ] `git diff --check` passes for the complete branch diff.
- [ ] Documented runnable commands changed by the branch are executed or validated against the
      already-built current targets, including XCCY examples and `xccy_perf` when referenced.
- [ ] A fresh DAL reviewer reports no blocking documentation or source-consistency findings.
- [ ] The final branch is pushed as a new documentation-only PR from merged `master`.

## Open Questions

None. The user selected the repository-wide audit and approved the documentation-only design.
