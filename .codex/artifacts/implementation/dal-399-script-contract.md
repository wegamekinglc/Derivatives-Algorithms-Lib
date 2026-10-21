# DAL-399 implementation and verification

Controlling contract: `../api-notes/dal-398-script-contract.md`.
Implementation base: `aca53fb1a11905cdc69fdecd832639b5471a705c`, including PR #606.
The working tree was clean at start; all pinned submodules were initialized without
changing their recorded revisions.

## Design and changed scope

- `dal/script/instrument.hpp`, `instrument.cpp` and `instrumentserialization.cpp`
  implement immutable definition/header/schema, source-authoritative defaults,
  complete effective bindings, clone/rebind and content identities. Source text
  stays exact; effective constants enter the parser through typed slots.
- `dal/calibration/types.hpp`, `protocol.hpp/cpp` and validation helpers own units,
  quote conventions, observations, case-sensitive axes, partial reports and
  copy-safe structured failures. Price amounts are not normalized by notional.
- `dal/storage/canonicaljson*` shares existing JCS/SHA-256 logic with the legacy
  quote-risk implementation. The implementation helpers remain local to that
  translation unit so their internal calls retain compiler optimization.
- Machinist produces all new enum headers, implementations and Java output.
  `event.hpp/cpp` add typed numeric overrides before AST construction; legacy
  callers use the same path with empty overrides.
- `tests/script/test_instrument.cpp` contains 21 focused tests, including an
  independent complete wire specimen from the approved contract. The methodology
  guide and CHANGELOG describe the new data boundary.

No native instrument is retained, no event-text substitution is used, and no
prepared cache is deserialized. Full receiver/template/affine admission, compiler
and context identities, native importers, execution plans, solvers, MC and public
factories remain downstream. The report here owns data-boundary axes/errors;
it does not invent solver metrics or claim portable calibration-session replay.
Its Write throws CalibrationPersistenceUnsupported.

Dates use the existing core Date_ range (1970-01-01 to 2149-06-05). Invalid or
unrepresentable ISO dates fail explicitly; expanding that representation is
outside this task. This is the only representation restriction beyond the
approved P0 contract.

## Red, green and refactor

Initial RED:

```sh
c++ -std=c++17 -fsyntax-only -I dal-cpp dal-cpp/tests/script/test_instrument.cpp
```

It failed because `dal/script/instrument.hpp` did not exist. The first immutable
binding test passed after implementation. Subsequent focused RED runs used
`cmake --build build/Release-linux --target dal_cpp_tests -j 8` followed by
`./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='<test selection>'`:

- ScalarSourceCannotBecomePriceDefinition: rejected missing PAYS at definition ingress.
- InvalidSourceDefaultHasDefinitionContext and LoadDefinitionFailureRetainsEnvelopeContext:
  corrected error ownership/path and instrument context.
- SourceNumericSpellingPreservedExactly: reused the DSL's numeric conversion
  rather than narrowing valid source spellings.
- QuoteCopyIsFrozenAndRoundTripsThroughBothArchives and QuoteUnitsConventionsAndFiniteValues:
  completed Archive::Store_::Done() and corrected FX token offsets.
- FailurePayloadOutlivesInputsAndContainsOffendingValue and RejectedTextProducesSafeOwnedDiagnostics:
  retained offending values and escaped invalid text in owned diagnostics.

- DalArchiveRequiresPortableJsonStringWithLoadContext: classified missing or
  mistyped archive envelopes at /json with an owning Load report.

Each selection failed for the stated behavior before its production correction.
All 21 new tests pass. Refactoring shared fingerprint
validation, constructor delegation and formatting retained green behavior.

## Verification scope

Release uses GCC 15.2, C++17, native AAD, static libraries and native-arch disabled.
Both `dal_cpp_tests` and `dal_public_tests` build. Directed checks cover:

- 215 core tests: the 21 new tests, legacy Product parsing/preprocessing, exact
  folding, historical replay, compiled parity, storage, quote-risk provenance/
  aggregation, and legacy GSR calibration.
- 28 public tests: ScriptArchiveTest (v1/v2 readers and v3 writer), ScriptTest,
  CurveSpecTest (defaults and existing curve APIs), PublicGSREuropeanTest.
- Portable definition and instrument JSON, complete bindings/origins, quote-copy
  provenance and both registered DAL archive objects reconstruct equal identities.
  Strict error cases include unknown/duplicate/missing fields, wrong numeric
  types/units, nonfinite values, fixed slots, versions and inconsistent defaults.

There are no example, CMake or facade changes; no full numerical/benchmark matrix,
Python, Windows Excel workbook or downstream pricing-equivalence claim is made.
Generated drift, exact commands, benchmark results and final head are included in
the PR and issue handoff.

Final commands (all successful):

```sh
cmake --build build/Release-linux --target dal_cpp_tests dal_public_tests script_perf rate_risk_perf -j 8
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptInstrumentTest.*:CalibrationProtocolTest.*:ScriptPreprocessorTest.*:ScriptTest.TestEvent*:ScriptPastReplayTest.*:ScriptExactFoldingTest.*:ScriptCompiledParityTest.*:QuoteRiskProvenanceTest.*:QuoteRiskAggregationTest.*:StorageTest.*:GSRCalibrationTest.*'
./build/Release-linux/dal-public/dal_public_tests --gtest_filter='ScriptArchiveTest.*:ScriptTest.*:CurveSpecTest.*:PublicGSREuropeanTest.*'
cmake --build build/Release-linux --target dal_check_generated -j 4
./build/Release-linux/dal-cpp/benchmarks/script_perf/script_perf
OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 python .github/scripts/check_benchmark_regressions.py --base-root build/dal399-perf/base-build --head-root build/Release-linux --output-dir build/dal399-perf/repaired-results --benchmarks rate_risk_perf --samples 10 --confirmation-rounds 2 --threshold-percent 4
```

## Performance selection

The changed cold parser entry maps to `script_perf`; its construction and
tree/compiled smoke completed. Sharing JCS/SHA maps to `rate_risk_perf`, which
validates actual legacy provenance callers. Other numerical kernels are unchanged.

The paired rate-risk comparison uses the baseline above, an isolated exported
baseline source/build, identical pinned dependency sources, Release/static/native
AAD builds, i9-13900HX, and OMP/BLAS thread counts 1. It alternates base/head process
order over two independent best-of-ten rounds with the unchanged +4% threshold
and six generic-joint 20% overhead gates.
No compilation overlapped the measured rounds. No other benchmark targets were
added to the regression gate. Script construction smoke is informational.

The initial extraction caused regressions of roughly 9–15% in three legacy
aggregation cases. Helpers were returned to local translation-unit scope; only
the required shared entry points cross the boundary. Affected correctness tests
were rerun (113 passed), and the same benchmark target/sampling policy was
repeated. The handoff retains both the initial and repaired measurements rather
than labeling the first attempt successful.

The repaired paired gate passed all cases. The three originally affected cases
were joint XCCY -1.27%, single curve -0.29%, staged XCCY basis -0.30%; none
exceeded the +4% threshold in both rounds, and all six generic-joint overhead
checks passed. Base executable SHA-256:
`cd7040d7fdb19f6615a000893dc84092d332d5724fc6aa6fa3aae69a558de801`;
repaired executable SHA-256:
`1f5cda30b68bbf6d412fa4b32c3f11377f6fa02de714b4cc70f994da3d3380d3`.
The subsequent archive-envelope-only correction is outside this caller; the
final rebuilt rate-risk executable retained that same hash, establishing reuse.
