# DAL Documentation Review

Review date: 2026-07-25

## Scope and method

The review covers every tracked Markdown file, every
`.codex/agents/*.toml` contract, and every
`.codex/skills/**/agents/openai.yaml` interface file. The baseline was 99 files;
the two required review artifacts bring the final scope to 101. The exhaustive
file-by-file classification is in
[DAL documentation inventory](dal-documentation-inventory.md).

Ground truth was established from, in descending order:

1. current public headers, implementations, bindings, examples, and tests;
2. build manifests, dependency locks, and CI workflows;
3. the repository-level agent contract and current Claude/Codex role contracts;
4. current published documentation;
5. active designs and retained specifications, which are evidence of intent but
   are not treated as shipped behavior.

The audit combined semantic source comparison with parser-based Markdown-link
and anchor validation, TOML/YAML/frontmatter parsing, role-set and methodology
index reconciliation, table/whitespace checks, and the repository documentation
checker and its unit tests.

The resulting patch makes evidence-backed semantic corrections in 15 existing
Markdown contracts or guides, applies table-only normalization to 23 additional
existing Markdown files, and adds these two review artifacts.

## Findings and dispositions

### Medium — web guidance described the wrong blocking boundaries

- **Files:** `.claude/rules/dal-web-code-style.md`,
  `.codex/skills/dal-web/references/backend-style.md`
- **Evidence:** `dal-python/src/bindings/value.cpp` releases the GIL around
  valuation; `dal-python/src/bindings/global.cpp` releases it around evaluation
  date access; `dal-web/backend/app/services/valuation.py` uses
  `asyncio.to_thread`. `dal-web/backend/app/services/db/store_db.py` performs
  synchronous selects and commits, while
  `dal-web/backend/app/services/db/session.py` accepts general SQLAlchemy URLs;
  `dal-web/backend/app/routers/products.py` shows the current direct store-call
  pattern inside async handlers.
- **Disposition:** corrected both contracts. A direct synchronous gateway call
  still blocks the event-loop thread; GIL release permits other Python threads
  to run but does not make the call awaitable. The current synchronous store
  interface and direct handler call pattern were preserved, but the contracts
  now state that database I/O can block the event loop, especially for remote
  backends, and require bounded operations and an explicit concurrency review
  when persistence scope changes. An obsolete async-spec reference was removed.

### Medium — Claude benchmark guidance was materially obsolete

- **File:** `.claude/agents/dal-performancer.md`
- **Evidence:** `dal-cpp/benchmarks/CMakeLists.txt`,
  `.github/workflows/cmake-linux.yml`, and
  `.github/scripts/check_benchmark_regressions.py`.
- **Disposition:** replaced missing `benchmark-*` artifact references and the
  claim that benchmarks were outside CTest. The contract now records 19
  registered benchmark targets, the eight fixed regression-gate targets, the
  11 informational targets, CI smoke discovery, and the current paired,
  multi-round gate and inconclusive-result rules.

### Medium — test contracts pinned stale Google Test counts and output

- **Files:** `.codex/references/run-tests.md`,
  `.claude/skills/dal-unit-test-skill/SKILL.md`,
  `.claude/agents/dal-tester.md`
- **Evidence:** the root `build_linux.sh` and `build_windows.bat` scripts invoke
  CTest; `dal-cpp/CMakeLists.txt` registers `dal_cpp_tests` through
  `gtest_discover_tests`.
- **Disposition:** replaced `409 tests from 46 test suites` with the actual
  CTest acceptance line and label/time summary, without pinning a count that
  varies by configuration. The Claude tester contract now tells authors to
  extend an existing relevant suite and to match the namespace convention of
  the target file, consistent with source and the dedicated test-style guides.

### Medium — Claude orchestration guidance had a broken route and incomplete roster

- **Files:** `.claude/agents/dal-orchestrator.md`,
  `.claude/agents/README.md`
- **Evidence:** `AGENTS.md` and `.claude/agents/dal-orchestrator.md` define the
  orchestrator as a dispatcher that plans, delegates, tracks, and reports but
  does not inspect files, artifacts, diffs, tests, gates, or PR state.
- **Disposition:** added the performance and simplification sidecar roles,
  corrected the example route to put critique before implementation, removed a
  duplicated implementation stage, and fixed the API-designer artifact path.
  The roster and handoff guide now assign the orchestrator only task tracking,
  delegation prompts, routing from specialist reports, and summary output.
  Artifact verification remains with each specialist, and commit/PR publication
  remains with an explicitly authorized publishing workflow. The
  self-referential historical change log at the end of the role contract was
  removed.

### Medium — tester isolation guidance relied on undefined execution order

- **File:** `.claude/agents/dal-tester.md`
- **Evidence:** `.claude/rules/unit-test-style.md` requires no mutable singleton
  state shared with other test files; Google Test supplies no alphabetical
  cross-test execution-order contract.
- **Disposition:** removed the recommendation to place writes after reads and
  the alphabetical-order claim. The tester contract now requires isolated or
  uniquely scoped state, restoration of unavoidable singleton mutations
  including assertion-failure paths, and tests that pass independently of
  execution order.

### Medium — supported Node versions were too broad

- **Files:** `CLAUDE.md`, `docs/installation.md`,
  `.claude/skills/dal-web-setup/SKILL.md`,
  `.codex/skills/dal-web/references/operations.md`
- **Evidence:** the installed Vite version's `engines.node` entry in
  `dal-web/frontend/package-lock.json` is `^20.19.0 || >=22.12.0`.
- **Disposition:** replaced `Node 20+` with the exact supported ranges.

### Medium — the IR-index example was not a canonical DAL name

- **File:** `docs/methodology/index_parsing.md`
- **Evidence:** `dal-cpp/tests/indice/index/test_ir.cpp` expects
  `IR:USD,LIBOR_3M_LCH`; the convention string is produced by
  `TradedRate_("LIBOR_3M_LCH")`.
- **Disposition:** replaced the non-canonical `IR:USD,Libor3M` example with the
  tested canonical form.

### Low — web operational and design contracts had small source drift

- **Files:** `.claude/skills/dal-web-setup/SKILL.md`,
  `.codex/skills/dal-web/references/operations.md`,
  `.claude/rules/dal-web-design.md`
- **Evidence:** `dal-web/scripts/start.sh` and `dal-web/scripts/start.ps1` use
  `uv sync --inexact`; `dal-web/frontend/src/styles.css` defines
  `--red-dim`.
- **Disposition:** documented the exact dependency-sync behavior and its reason,
  and added the missing design token to the Claude palette.

### Low — Copilot guidance over-required `uv` for the standalone Python binding

- **File:** `.github/copilot-instructions.md`
- **Evidence:** `dal-python/pyproject.toml` defines the PEP 517 build backend;
  `dal-python/README.md`, `dal-python/build_wheel.sh`, and
  `dal-python/build_sdist.sh` document standard pip installation of built
  artifacts; the root `build_linux.sh` retains a pip fallback for Python test
  dependencies. The web backend has its own uv lock and startup flow.
- **Disposition:** made `uv` mandatory for `dal-web` and recommended, but not
  mandatory, for standalone `dal-python`.

### Low — repository documentation automation covers only part of this audit scope

- **Files:** `.github/scripts/check_docs.py` and its tests are source evidence,
  not files changed by this documentation-only review.
- **Evidence:** the repository checker validates 39 Markdown files, while the
  final review scope contains 89 Markdown files. An independent parser-based
  pass over the complete inventory found no broken direct local links or
  anchors.
- **Disposition:** no automation code was changed because it is outside the
  requested write scope. Expansion of the checker is an explicit follow-up
  decision below.

### Low — legacy pipe tables did not consistently follow the repository style

- **Files:** 27 current, contract, active-design, experimental, and retained
  historical Markdown files across `.claude/`, `.codex/`, component READMEs,
  and `docs/`.
- **Evidence:** `.claude/rules/code-style.md` and
  `.codex/references/code-style.md` require every pipe-table column to be padded
  to its longest cell, with separator dashes matching the resulting width.
- **Disposition:** mechanically normalized all 84 detected pipe tables in the
  complete 89-file Markdown scope. Cell text, row order, and alignment semantics
  were preserved.

## Artifact and status decisions

- `.claude/designs/api-shape-dedup.md` and
  `.codex/artifacts/designs/api-shape-dedup.md` are byte-identical active
  designs. Their explicit status is “Design only. No code changes. Awaiting
  user approval.” They were preserved and were not described as current
  behavior.
- The joint-AAD, PDE, web-persistence, simultaneous-multi-curve,
  compiled-evaluator, and XCCY documents are retained implementation history.
  Current behavior was validated against source and current methodology pages;
  historical paths and sequencing inside those records were not rewritten as
  present-day instructions.
- The two files under `docs/experimental/` remain explicitly non-normative.
  Their location and `docs/README.md` classification were preserved.
- The four `docs/superpowers/` records are historical design/implementation
  material. They remain outside the published `docs/README.md` index by design.
- No `CHANGELOG.md` entry was added: these changes correct documentation and
  agent contracts without changing product behavior or a public API.

## Intentional Claude/Codex differences

- The role set is identical: both surfaces define the same 10 DAL roles.
  Claude roles are verbose Markdown documents with YAML frontmatter, optional
  color/tool declarations, and Claude-specific artifact paths. Codex roles are
  compact TOML contracts with explicit delegation-authorization and
  `.codex/artifacts/` rules. Converting either representation to the other
  would discard platform-specific behavior.
- Claude exposes five user-invocable Markdown skills. Codex exposes two skills
  with skill-specific UI metadata and uses seven reusable reference documents.
  This is a platform packaging difference, not missing parity.
- `.claude/rules/git-commit-pr.md` and
  `.codex/references/git-commit-pr.md` are byte-identical, as are the two
  unit-test-style documents. Other reference pairs intentionally use their
  platform's artifact roots and invocation language.
- `.codex/skills/dal-agent-team/references/shared-rules.md` is a compatibility
  pointer to `AGENTS.md`, not a duplicated rule set.
- `README.md` is a concise project overview and intentionally links a selected
  set of methodology pages. `docs/README.md` and `CLAUDE.md` are the exhaustive
  16-page methodology indexes.

## Unresolved, decision-dependent follow-ups

1. **Documentation-checker coverage.** Decide whether a separate code change
   should extend `.github/scripts/check_docs.py` from its curated 39-file set to
   every tracked Markdown document. This review supplies a working full-scope
   validation model, but changing CI automation was out of scope.
2. **Historical-document governance.** Decide whether implemented
   Claude specs, designs, API notes, and critiques and the
   `docs/superpowers/` records should remain in place, move to a common archive,
   or be removed after a retention period. Reclassification or relocation
   requires a repository policy decision; silently rewriting historical paths
   would erase useful execution evidence.
3. **API-shape deduplication.** The active design still awaits approval.
   Documentation must continue to describe the current duplicated validation
   behavior until the design is accepted and implemented.

## Validation record

The final validation commands and observed results:

- `python3 .github/scripts/check_docs.py` — passed for the checker's curated 39
  Markdown files.
- `python3 -m unittest discover -s .github/scripts/tests -p 'test_check_docs.py' -v`
  — 18 tests passed.
- parser-based full-scope Markdown audit — 89 documents and 365 parsed links or
  images; zero direct local-link, anchor, table-structure, whitespace, or final
  newline errors.
- TOML/YAML/frontmatter and set-reconciliation audit — 10 TOML contracts, two
  YAML interfaces, 17 Markdown frontmatter blocks, 101 inventory entries, 10
  roles per platform, and 16 published methodology pages; zero errors.
- exact table-format idempotence audit — 84 tables across 89 documents; zero
  alignment-drift files.
- review-artifact path audit — 174 path mentions representing 131 unique paths
  or path patterns; zero missing. The evidence subsections contain 27 path
  mentions representing 26 unique existing paths.
- semantic/table-only classification audit — 38 modified existing Markdown
  files: 15 with semantic corrections and 23 with table-only normalization.
- `git diff --check` — passed.
