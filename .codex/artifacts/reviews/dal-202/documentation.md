# DAL-202 / F5 current documentation reconciliation

Documentation now describes the integrated F5 source and advisory performance
policy. This report replaces the stale active documentation handoff. Parent
DAL-202 acceptance and mandatory DAL-231 independent review of the published
revision remain required; earlier review approval does not approve this head.

## Revisions and scope

- Accepted tester publication: `9f6a3653d352fca268e22578648e53431ee10b37`, tree
  `4b68370715a6dd633a9ed66880fe6eb74a7ae4c1`.
- Independently tested commit: `3bacea1f1671deaccb09617b60300820b5ab4711`, tree
  `d9fa401afaf0da665aa8c6b67c82cbc3319eb956`. Only testing.md changed afterward.
- Integrated F4 ancestor: `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2`.
- Existing branch: `feature/dal-202-compiled-observations`.
- [PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372)
  remains open, owner-set ready for review, with base master. No stacked
  dependency remains.

The clean starting checkout and GitHub head match the accepted publication.
Final published SHA/tree, remote/PR identity and the single CI snapshot are
recorded separately in the attached publication record and delivery comment,
avoiding a self-referential commit hash in this file.

Changed paths:

- `docs/methodology/script_engine.md`
- `docs/methodology/aad.md`
- `docs/README.md`
- `CONTRIBUTING.md`
- `dal-python/benchmarks/README.md`
- `.codex/artifacts/reviews/dal-202/documentation.md`

Everything outside these six documents equals the accepted tester head,
including production, tests, workflow/configuration, generated files, bindings,
executable examples and dependency pointers. No methodology document was added,
removed or renamed. The docs index gains the native tape topic; the CLAUDE
methodology list requires no change. Published example fences are unchanged.

## Source reconciliation and decisions

Read the complete script/AAD methodology, docs index, CHANGELOG, Python benchmark
README, contributor guidance, root overview, benchmark workflow reference, and
current implementation/testing/documentation reports. Checked current public
headers, bindings, examples and relevant tests against the implementation.
There is no `dal-cpp/benchmarks/README.md`; its CMake inventory and the shared
benchmark reference supply native benchmark guidance.

Non-trivial discrepancies and changes:

- `CONTRIBUTING.md` still called the paired performance comparison a gate.
  It now states that reports are advisory and preserves the two-round,
  ten-sample, strict 4% rule, Sobol ceiling, nonzero comparison exits and failed
  evidence. Both stable gates exclude Benchmarks; required correctness and
  documentation checks remain. The Python benchmark README states the same
  CI/merge/delivery policy explicitly.
- The script guide now quotes the actual AAD driver diagnostic:
  `UnsupportedExecutionMode: AAD mode, smoothing, or compiled/tree mode differs from preparation`.
  It distinguishes the outer scalar-type guard and clarifies that an omitted
  compiled argument resolves to tree, rather than inheriting preparation.
  Source: `dal-cpp/dal/script/simulation.hpp`; regressions:
  `dal-cpp/tests/script/test_past_replay.cpp`.
- Legacy compiler-produced streams without an observation plan or historical
  mode use the legacy dispatcher. Direct stream construction does not certify
  that optimization; observation loads still require a plan. Shared prepared
  instruction and payment mappings remain intact. Source:
  `dal-cpp/dal/script/event.hpp` and `dal-cpp/dal/script/visitor/compiler.hpp`.
- The allocation discussion now states CTest registration, 8193 double
  evaluations including the first, ordinary/aligned positive controls, and
  passive versus typed evaluator seed storage. Source:
  `dal-cpp/dal/script/visitor/evalstate.hpp`, `dal-cpp/CMakeLists.txt`, and
  `dal-cpp/test-support/test_script_observation_allocations.cpp`.
- The AAD guide describes native block storage, per-tape allocation,
  three-input call placement through the shared allocator, and rollover/reuse
  limits. It does not promise allocation-free AAD. Source:
  `dal-cpp/dal/math/aad/tape.hpp`, `dal-cpp/dal/math/aad/blocklist.hpp`, and
  `dal-cpp/tests/math/aad/test_tape.cpp`.
- The prior active report's stacked base, obsolete SHAs/test totals and
  unchanged-performance-gate claim have been replaced by current evidence.

The existing core contract remains accurate: syntax-wide prefetch and sealed
same-plan observation IDs; exact midnight/today policy; hard history with PAYS
evaluation/discard; locally rebuilt typed AAD seeds and live parameters; exact
and fuzzy model-aware branch retention without tolerance-domain processing;
continuous kernels with default/explicit epsilon; final IF/constant metadata
and compilation before workers. `AAD::PayoffRoot` still reuses only a terminal
native post-mark node and otherwise records payoff plus a registered zero.
Legacy AAD still rejects nonexpired raw products with past events. The legacy
condition visitor preserves eager booleans and only scans for them when a
condition would otherwise fold; model-aware preparation skips that visitor.
No contract or example deviation requiring API/spec/critic review was found.

**CHANGELOG decision: retain the existing 2026-09-14 Script compiled
observations capability entry unchanged.** It already covers the significant
capability and exact/fuzzy preparation semantics. Internal allocation/dispatch
changes, CI policy and documentation reconciliation do not qualify for another
entry. Earlier capability entries remain historical records. Root README and
the shared benchmark reference already state advisory reporting correctly.

## Fresh documentation validation

Commands, from the repository root:

```bash
python3 .github/scripts/check_docs.py
python3 ../doc-evidence/verify_inputs.py
python3 ../doc-evidence/validate_docs.py
git diff --check
git diff 9f6a3653d352fca268e22578648e53431ee10b37 --name-status
git diff --cached --name-status
git diff --cached --check
```

The repository checker passes for **58 Markdown files**. Supplemental checks
cover all six changed documents, including the Python benchmark README outside
the checker's default inventory: local links/anchors, exact table padding,
trailing whitespace, final newlines, unchanged published example fences, and
zero diff outside the six-path allowlist. Attached scripts and command logs
record these fresh checks. No product or timing tests were rerun for prose edits.

## Accepted independent testing evidence

Downloaded DAL-229's report/archive/checksum through Multica CLI. Archive SHA256:
`6e3ad7c9022c8e9a34e7d4fb77c2a47ac3cbc209b82f70288bbab38d724500b7`.
Freshly verified all **143 internal hashes**, **837 source hashes** against the
accepted Git revision, report equality between attachment/archive/Git, both
revision trees, F4 ancestry, XML counts and the recorded CI scenario results.
Source verification honors the repository's CRLF checkout attributes for its
two PowerShell scripts; their raw Git blobs use LF.
Read the tester's expanded commands, raw outputs and RED/GREEN evidence.

The following are inherited independent executions by DAL-229, not new
doc-writer test runs:

- Full Linux native configure/build/install/CTest **1753/1753**, including
  Python **402/402** within one CTest entry.
- Native/Adept/CoDiPack/XAD focused **457/456/456/455**, plus separate tape
  **12/4/4/4**; all **33 parity/fuzz cases** and **27 lifetime combinations**
  per backend, with 8193 paths and threads `{1,2,4}`.
- Original exact **4**, tiny-divisor **6**, isolated allocation **2**;
  Clang ASan/UBSan **403** plus allocation **2**; GCC static/shared and Clang
  sanitized LTO consumers **3**, installed-package consumer **1**.
- CI tools **163 run, 157 pass, 6 existing Windows skips**. Two added actual
  Bash tests pass **86 current gate scenarios**. Original blocking workflows
  and three weakened correctness variants each produce 10 expected failures;
  all **430 executions** are retained. Complete YAML graphs and comparison
  AST checks preserve numerical validators, calculations and exit codes.

Under Cheng Li's controlling instruction, performance reports are advisory,
not CI, merge or F5 delivery gates. Historical C++ **61/63** and Python
**87/90** remain failed reference measurements. No relabeling, complete timing
rerun, DAL-223 revival or private NextBlock experiment is part of this work.

## Limits and handoff

Named valuation settings remain core-only. F6-F8 public facade/Python/Excel
settings, default-index archive projection and FIX JSON schema support remain
unavailable. Explicit BS/Dupire binding supports one ordinary future EQ;
historical EQ/FX rules do not imply future FX/IR/composite/delivery/multi-asset
support. Global history capture remains sequential and non-atomic.

Allocation probes cover native double C++ requests, not arbitrary malloc or
all AAD allocations. Indexed observation access is source-inspected. Compiler
failure injection occurs immediately before bytecode construction. No local
Windows/MSVC/XLL or full alternate-backend public/Python/Excel execution is
claimed. Examples were built/installed by the tester, not exhaustively run.

DAL-230 returns in_review for parent acceptance. Parent restores mandatory
DAL-231 independent review; protected merge and required correctness checks
remain outstanding. Capture hosted CI once after publication, without
watch/sleep/poll. No merge, closing intent, issue closure, peer chain or F6
advancement. If the parent is idle after delivery, invoke its dedicated rerun
and record the returned run ID.
