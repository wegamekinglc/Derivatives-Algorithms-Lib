# DAL-200 F3 documentation decision

Documentation reconciliation is complete for the core double/tree FIX
capability. The changes correct preparation-only claims, explain current
valuation behavior and compatibility limits, and add a qualifying CHANGELOG
entry. This is the documentation handoff, not independent reviewer acceptance
or F3 completion.

## Revision and scope

- Starting implementation/test SHA: `c64c6102ba84d88f0fe6483574251daf06dd76c2`.
- The dedicated local branch is `agent/dal-doc-writer/2fc6ba200eab`; the
  checkout was clean and matched the actual PR head before editing.
- F2 ancestor `ec8b0072fbf70dab814a543edc625c8e0bf77efa` is present.
- Publication continues the existing branch
  `feature/dal-200-eq-observation-slots` through a non-forced
  `git push origin HEAD:feature/dal-200-eq-observation-slots`.
- Existing draft [PR #369](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369).
  The DAL-218 final comment records the exact before/after publication SHAs;
  the successor containing this report preserves the starting production and
  test trees. No cherry-pick or duplicate PR is required.

Exactly five files form this documentation change:

- `docs/methodology/script_engine.md`: model-aware core entry points, explicit
  EQ binding, retained observations and payment addresses, historical replay,
  today/expiry behavior, preparation and execution boundaries.
- `docs/methodology/index_parsing.md`: replace the blanket nonexpired FIX
  rejection with the actual core capability and remaining limits.
- `docs/README.md`: index the new content in the existing script methodology.
- `CHANGELOG.md`: record significant core valuation capability and historical
  SPOT/AAD compatibility changes; make the older preparation-only entry
  explicitly historical.
- `.codex/artifacts/reviews/dal-200/documentation.md`: this active evidence.

No methodology document was added, removed, or renamed. The existing
`CLAUDE.md` methodology link remains valid and needs no change. No C++, tests,
executable examples, bindings, generated files, agent definitions, build/CI
configuration, or submodule pointers are changed.

## Non-trivial discrepancies resolved

The original script and index notes said every nonexpired prepared FIX product
was unsupported and numeric visitors rejected all FIX nodes. That is true only
for preparation without a model or unprepared execution. Model-aware
preparation now supplies the exact double evaluator with an observation plan.
The documentation identifies both overloads rather than implying that the
old history-only overload acquired executable model state.

The original note also said SPOT was never collected as a named observation
and historical state evaluation was unavailable. Core contract settings now
bind zero-argument SPOT to an explicit default index and deduplicate matching
FIX uses. Named double replays historical state once. Unbound historical SPOT
throws instead of reading 30. Legacy AAD rejects a nonexpired product with
any past events, including FIX-free historical assignments, because active
historical state reconstruction is unavailable.

Generic domain/fuzzy/compiled descriptions are now qualified as legacy
behavior. Named double uses the collected, unoptimized AST; named AAD,
compiled and fuzzy modes remain rejected. The public facade and Python/Excel
signatures remain unchanged and cannot pass the core settings or snapshots.
Public C++/archive/Python/Excel projection remains assigned to F6-F8; this
report does not promote those stages or imply end-to-end availability.

## Ground truth and acceptance mapping

Source was reconciled against the parent DAL-200 design/matrix, the accepted
`implementation.md` and `testing.md`, and DAL-217's final thread
`01a097ca-f377-73e7-b453-01e121751667`. Each edited published file was read
in full. The following evidence controls the current-state wording:

- `dal-cpp/dal/script/preparation.hpp` and `preparation.cpp`: both preparation
  overloads, collection before optimization, exact midnight, today policy,
  binding/model setup before history, deduplication and sealed doubles.
  Covers T03/T05/T06/T09/T11/T12/T31 and the unchanged F2 snapshot contract.
- `dal-cpp/dal/model/base.hpp`, `blackscholes.hpp`, `dupire.hpp`, and
  `dal-cpp/dal/math/aad/sample.hpp`: explicit `spot` identity, plain built-in
  EQ capability, one future index, sorted sample validation, requested
  outputs, valid empty-forward samples and deterministic input validation.
- `dal-cpp/dal/script/observationplan.hpp`, `preparation.hpp`, and
  `dal-cpp/dal/script/visitor/evaluator.hpp`: integer history/model addresses,
  retained earlier observations and the independent payment sample map.
  T10/T24/T32 include retained F=120 versus payment spot=999, repeated-read
  cancellation, `120/4 + 120/5 = 54`, and discounted known fixing 80.
- `dal-cpp/dal/script/event.cpp`, `settings.hpp`, `settings.cpp`, and
  `dal-cpp/dal/script/visitor/pastevaluator.hpp`: double past-state replay,
  consumed historical PAYS operands, same-date ordering, SPOT default
  identity and invalid-configuration guards (T17/T27).
- `dal-cpp/dal/script/simulation.hpp` and `simulation.cpp`: integrated core
  entry, positive path count, aggregate-sum convention, expired early return,
  zero-dimensional RNG/BB avoidance, named/legacy AAD boundaries, and task
  draining on numerical failure (T25/T26/T31).
- `dal-cpp/tests/script/test_observation_simulation.cpp`: controlled 123/200/160
  oracles, retained fixing and independent numeraires, both-adapter capability
  barriers, today policies, frozen worker storage, legacy compatibility and
  mode rejection. The published test blob remains
  `12efeeb0fa35f573eda49fe86651f2fb38584f29`.
- `dal-public/src/value.hpp` and `value.cpp`,
  `dal-python/src/bindings/value.cpp`, `dal-excel/src/__value.cpp`, and
  `dal-cpp/dal/script/event.hpp`: no public named settings/snapshot/default
  arguments or archived default index. The existing script example was also
  checked; no new executable example is introduced.

The note preserves exact-date policy, no lookahead including dead branches,
no fallback for missing history, immutable caller-selected snapshots, and
the global capture limitation. Sequential sequence snapshots are not an atomic
joint market snapshot; capture must not race fixing writes. Historical EQ
delivery and inverse FX remain supported without implying future delivery,
FX, IR/composite or multi-asset outputs. Intraday fixing cutoffs are not added.

## CHANGELOG decision

Add one entry under the existing `2026-09-13` date. Executable core double/tree
valuation with retained model observations is a significant capability, and
the historical SPOT placeholder removal plus legacy historical-AAD rejection
change consumer-visible behavior. These meet the documentation contract even
though the public facade and named AAD/compiled/fuzzy projections are deferred.
The old historical-preparation entry retains its facts as an introduction-time
record. Published docs contain no DAL stage, PR, test-repair, or delivery
narrative. The IRN diagnosis stays in active testing evidence; no claim of
schedule-independent IRN paths is published.

## Verification

Executed from the repository root:

```bash
python3 .github/scripts/check_docs.py
git diff --check
git diff --cached --name-status
git diff --cached --check
git diff c64c6102ba84d88f0fe6483574251daf06dd76c2 --exit-code -- dal-cpp dal-public dal-python dal-excel
git rev-parse HEAD:dal-cpp/tests/script/test_observation_simulation.cpp
```

The documentation checker passes for 64 Markdown files, covering local links
and anchors, Markdown table structure, trailing whitespace, math macros and
the repository's documentation contracts. A separate inline Python audit uses
the checker's table parser to compare each table with the exact padded-column
format required by code-style: all three tables in the affected methodology
files pass. All five changed Markdown files end with a newline. Diff checks
pass and staging contains only the five files listed above. The production
and test directories are byte-for-byte unchanged from the starting revision.

No C++ build or runtime tests were rerun for this prose-only change. Accepted
DAL-217 evidence reports GCC 15.2/AADET 342/342 focused and 1,663/1,663 canonical
CTest; Clang 21.1.8/CoDiPack 342/342 and 1,655/1,655 CTest; each backend has
6,840 passing shuffled focused executions. These are attributed tester results,
not executions by the doc-writer. Shared-path parity preserves the `1e-8`
oracle without claiming schedule-independent independent parallel IRN runs.

## Remaining handoff

DAL-218 is delivered for acceptance in `in_review`. Parent reconciliation,
the existing DAL-219 independent reviewer and final current-head required
checks remain outstanding. The PR stays draft; no merge, parent closing lines,
F3 closure, reviewer activation or F4 advancement is performed. CI snapshots
are observations, not final acceptance; no waiting or repeated CI polling is
performed. After the final child comment and status, the parent continuation
uses the authorized active-run/rerun protocol.
