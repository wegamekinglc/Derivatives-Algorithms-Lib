# DAL-199 F2 implementation handoff

Implementation is based on F1 merge `65c6b87a088ba12cddfa57dcc111250c9bfae16a`
on `feature/dal-199-historical-fixing-preparation`. The containing Git commit is
the implementation handoff; independent testing, documentation, review, and
publication remain owned by the orchestrator's subsequent specialists.

## Implemented behavior

- Parsing retains all event dates without reading the global evaluation date.
  `PartitionEvents` is a separate one-time operation. Legacy `PreProcess` and
  the new core `PrepareScript` capture their evaluation date at preparation.
- `PrepareScript(ScriptProductData_, settings, optionalSnapshot)` builds a fresh
  product and immutable observation plan on every call. It traverses original
  syntax before optimization, including dead branches and historical PAYS RHS.
  A request records canonical `Index_::Name()`, exact midnight time, every source
  location, and stable event/statement/node use identifiers.
- Logical requests deduplicate by canonical name and exact timestamp. Historical
  EQ (including delivery identity) and FX are the only accepted adapters. Today
  defaults to model; `REQUIREHISTORICAL` selects history. Lookahead is rejected in
  every syntax branch, including wholly expired products.
- Default preparation snapshots only the historical dependency closure. Global
  sequence reads are cached per name, including FX direct and reverse names.
  Explicit snapshots never access global history. The snapshot bridge projects
  raw `Values()` by exact dependency key into a dedicated, non-null immutable
  `FixingsAccess_` environment; it never synthesizes a direct FX quote.
- Exactly one final virtual `Index_::Fixing` runs per unique historical request.
  FX inversion remains in `Fx_::Fixing`; positivity, finiteness, and reciprocal
  consistency are enforced. EQ zero and negative finite values remain valid.
  Only immutable doubles survive preparation; the snapshot environment expires.
- Snapshot failures carry a structured dependency key for source attribution.
  Lookup failures use `MissingFixing`; invalid final values use `InvalidFixing`.
  Preparation throws without publishing a partial product or plan.
- Internal scoped observers measure global `History(name)`, final virtual
  `Fixing`, and accepted worker submissions. Raw FIX products now fail at both
  double/AAD simulation entry points before model access or submission. AAD
  caller-side model allocation/initialization validates the shared setup before
  workers. Existing task groups still own/drain accepted futures.
- The core prepared-product simulation overload returns zero value/risk for a
  structurally valid wholly expired product, before Allocate/GeneratePath or
  worker submission. All other prepared execution explicitly reports
  `UnsupportedExecutionMode` until the later execution stages.

## Scope decisions

The F2 core preparation entry requires a PAYS statement, so empty tables,
definitions-only products, and assignment-only products fail structurally even
when expired. The existing direct-assignment legacy product API is preserved.

Model output/sample mapping, AST/bytecode observation reads, exact/fuzzy settings,
historical state evaluation and AAD rebuilding are intentionally not implemented
here. Unresolved requests have no historical value ID; no provisional future
output interface is exposed. A plan is rebuilt from `ScriptProductData_`, never
from a previously optimized product. Old SPOT() stays on the guarded legacy
model path. Public/Python/Excel feature entry points are unchanged.

Default global snapshots are sequential per-sequence captures, not atomic market
snapshots. Concurrent fixing writes during capture remain outside the supported
contract; callers can provide an explicitly fixed snapshot.

## RED/GREEN evidence

All builds use `cmake --build build/Release-linux --target dal_cpp_tests -j12`.
Configuration was `cmake --preset=Release-linux -S . -B build/Release-linux
-DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=ON`; initial checkout dependencies
were initialized at their pinned submodule SHAs. The TodayFixingPolicy enum was
generated with `cmake --build build/Release-linux --target dal_generate -j8`,
including its generated C++ and Java files.
The existing explicit generation-normalization manifest includes the two new
C++ enum outputs so whitespace checks and regeneration remain reproducible.

- Partition RED: `--gtest_filter=ScriptFixingPreparationTest.TestEvaluationDateCapturedAtPreparation`
  failed because an event parsed before a date change stayed expired (0 dates
  versus expected 1). GREEN passes. Logs:
  `build/dal-199-partition-{red,green}.log`.
- Preparation API RED: the 300-use/3-event unique-history test did not build
  because the new preparation/observer API did not exist. GREEN proves 1 logical
  request, 300 uses, 1 final virtual call and 1 underlying EQ sequence read.
  Logs: `build/dal-199-preparation-red-build.log` and
  `build/dal-199-preparation-green.log`.
- Barrier RED: `--gtest_filter=ScriptFixingPreparationTest.TestUnpreparedFixingRejectedBeforeModelAccess`
  reached the model factory instead of reporting `PreparationRequired`.
  GREEN covers double/AAD and tree/compiled, with zero accepted submissions.
  Logs: `build/dal-199-barrier-red.log` and `build/dal-199-final-focused.log`.
- Source diagnostic RED: `--gtest_filter=ScriptFixingPreparationTest.TestSnapshotFailureNamesFailingUse`
  attributed an invalid second quote to row 1; GREEN attributes it to row 2.
  Snapshot failure keys are structured rather than inferred from error text.
  Logs: `build/dal-199-source-{red,green}.log` and final focused log.

Final focused command:

```bash
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptFixingPreparationTest.*:FixingEnvironmentTest.*:FixingSnapshotTest.*:ScriptObservationTest.*:ScriptTest.*:ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*'
```

Result: 256 tests across 7 suites passed. Logs:
`build/dal-199-final-build.log`, `build/dal-199-final-focused.log`.
This includes 15 preparation and 3 raw-environment tests. Existing task-group
and script-parity regressions pass. Changed source ranges and new files were
clang-formatted; `git diff --check` passes. Complexity output is retained in
`build/dal-199-complexity.log`.
`cmake --build build/Release-linux --target dal_check_generated -j8` also passes
(`build/dal-199-check-generated.log`), as does the exact staged whitespace check.

The full native/public suites, other AAD backends, Windows build, independent
review, and performance gates are not claimed at this implementation handoff.
The F9 path/thread/mode cross-product, model output valuation, historical AAD
replay, model/compile fault injection across future prepared modes, and evaluator
allocation-free observation reads remain later-stage integration requirements.
