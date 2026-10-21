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

## CI repair on PR #607

Repair input head: `aa4363cbbb305df49f15d262551567962d6b49e7` on the existing
`agent/dal-implementer/50614838b462` branch. This follow-up addresses the compiler
and Codacy diagnostics from that head. It does not close the separately reported
JSON-depth/parser-error review findings or documentation-index follow-up.

### Diagnostic evidence and design

- GCC 14 warning-clean job `119018220881` fails at all five ReadArray call sites
  with `-Werror=dangling-reference`. The returned array belongs to its JSON input;
  the compiler associates the reference with the temporary diagnostic path.
  CheckArray now validates without returning a reference, and callers traverse
  the array directly from the owning JSON field. No warning suppression is added.
- MSVC job `119018216773`, all-diagnostics job `119018216960` and Windows wheel
  job `119018232847` fail with C4800 at `Check(seen.count(copy.first), ...)`.
  Membership conditions now explicitly compare the count with zero.
- Codacy check `119018399119` reports six complexity findings, not an access or
  service failure. Lizard 1.23.0 reproduces the exact counts: ValidId 14,
  ValidKey 9, ValidateDefinition 47, ApplyBindings 12, ValidateParameter 14,
  ReadBinding 9. Small local validation functions preserve the condition order,
  short-circuit guards, field paths and owned diagnostic context. Coupon detection
  is computed once and shared by parameter and measure validation.

Production edits are limited to `dal/calibration/validation.cpp`, its internal
header, `dal/script/instrument.cpp` and `instrumentserialization.cpp`. The
existing test file gains four characterizations covering ID alphabet/length,
open/closed/equal parameter bounds, all five JSON array type errors, and partial
QuoteCopy provenance. They pass on the original validation logic before its
complexity refactor and after the refactor. This warning/refactor repair uses
compiler/static-analysis RED gates rather than changing runtime behavior.

### RED and GREEN

The following focused RED command failed with the same five GCC diagnostics;
after CheckArray extraction the identical command passed:

```sh
g++-14 -std=c++17 -fsyntax-only -I dal-cpp \
  -I dal-cpp/externals/rapidjson/include -I dal-cpp/externals/eigen \
  -Wall -Wextra -Wpedantic -Werror \
  -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
  -Wno-dangling-else -Wno-reorder -Wno-switch \
  -Wno-missing-field-initializers -Wno-parentheses -Wno-unused-variable \
  -Wno-redundant-move -Wno-pessimizing-move -Wno-sequence-point \
  -Wno-implicit-fallthrough dal-cpp/dal/script/instrumentserialization.cpp
lizard -C 8 -w dal-cpp/dal/calibration/validation.cpp \
  dal-cpp/dal/script/instrument.cpp dal-cpp/dal/script/instrumentserialization.cpp
```

The Lizard command initially lists exactly the six Codacy findings; after the
refactor it reports none. All 81 functions in those files have CCN <=8, with no
configuration changes or exclusions. Actual MSVC RED evidence is the CI logs;
MSVC execution is left to the new-head Windows jobs, not claimed as a local pass.

GREEN verification:

```sh
cmake --build build/Release-linux --target dal_cpp_tests rate_risk_perf -j 12
./build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='ScriptInstrumentTest.*:CalibrationProtocolTest.*'
cmake --build build/warning-clean --target dal_cpp dal_public -j 12
```

The focused selection passes all 25 tests. The warning-clean tree uses GCC 14.3,
Debug, the exact warning flags above, both core/public libraries, and tests,
examples, benchmarks and Python disabled, matching the failing CI job. Release
uses GCC 15.2 with the same pinned dependencies and native AAD configuration as
the initial implementation. Formatting and diff checks pass. The broader 215
core/28 public/generated/frontend evidence above remains prior evidence, not a
claim that those checks were all rerun in this repair.

### Performance evidence reuse

The repair does not change legacy Product/archive, JCS/SHA, quote-risk or parser
implementation. The changed data-boundary functions are absent from the linked
rate-risk benchmark. Rebuilding rate_risk_perf before and after the complexity
refactor produces the identical local SHA-256
`a9dfaf72ab416f8223da429447e91fa64754a46ac49ee732e1a31607b4d787c6`.
The local path differs from the original evidence build, whose executable embeds
different source paths; this is a local before/after equality, not a claim of
equality with the archived `1f5cda30...` executable. Together with unchanged
legacy caller/dependency/configuration provenance, it establishes that this
repair preserves the previously accepted paired performance evidence. No new
timing matrix is needed or claimed.

## Focused review repairs

Input head: `4eab56f91e80fbf4b948790363e672df7858d413`; review evidence:
`01a15c86-cb26-70f5-9c69-403650f4af4b`. This follow-up addresses the two review
blockers and the three requested methodology-index links on the same PR/branch.

### Bounded portable JSON loading

The new portable loaders run RapidJSON's iterative SAX reader with a 128-container
nesting budget before constructing a DOM or recursively converting it. The
preflight accepts valid strings/escapes independently of their bracket characters;
duplicate-field/schema validation remains in the existing conversion path. The
50 MiB input bound is unchanged. A budget failure returns InvalidCalibrationArchive
with an owned Load-stage report and the observed depth at rejection.

Two process-isolated tests reproduce SIGSEGV at depth 12000 for arrays and objects
before the fix. After the fix they cover depths 129/12000/30000, both container
types and both definition/instrument loaders, exiting normally with the required
owned failure. Additional coverage checks the inclusive depth-128 boundary and
escaped quotes/backslashes/brackets inside a valid contract's route string.

### Contextual script parse failures

A caller-owned ParseContext_ is passed through new core parser/Product overloads;
the existing overloads remain and pass null. The parser records the current
top-level statement ordinal and the failing token's source location before
rethrowing the original exception. No context pointer is retained in a product,
and Exception_/ScriptError_ are unchanged.

Only the new contract boundary translates the legacy DAL parser exceptions into
CalibrationError_. It copies row/date/statement and cause into the owned payload,
with `/source/rows/<zero-based-index>/text`; portable load prefixes the definition
path and retains its instrument/load context. HasPays validation stays outside
the parser catch so NoSharedPriceDefinition keeps its existing error code.

Four tests fail before the fix: factory, portable instrument load, portable
definition load, and same-date merged events. They show the unstructured N^
exception and the imprecise N* path. After the fix they pass for both expressions,
including one-based source row 5, event date 2027-10-21, statement 0 (or 1 after
merging a prior statement). Payload assertions occur after the action's inputs
have unwound. A fifth characterization confirms the legacy Product still throws
plain Exception_ for N^ and ScriptError_ for N*.

### Documentation and validation scope

The guide is linked directly from `docs/README.md`,
`docs/methodology/README.md` and the explicitly authorized CLAUDE methodology list.
The guide states the nesting budget and available parser-error context without
expanding the implemented financial scope. There are no enum, CMake, facade,
example, solver, JCS/SHA or quote-risk changes.

RED commands (both exit 1 after successful test builds):

```sh
./build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='ScriptInstrumentTest.TestArchive*DepthIsBoundedBeforeParsing' \
  --gtest_death_test_style=threadsafe
./build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='ScriptInstrumentTest.Test*Parser*'
```

The same focused commands pass after their respective fixes (2 depth tests and
5 parser/legacy tests). Depth tests select threadsafe subprocess execution in
the committed test helper. The complete affected selection additionally covers
existing parser/preprocessor, event construction, historical replay, folding,
compiled parity and public Product/archive compatibility. The unchanged numerical
calibration and quote-risk suites reuse the prior accepted evidence.

The added preflight initially rejected a UTF-8 BOM accepted by the original DOM
entry. A focused BOM regression failed with InvalidCalibrationArchive, then
passed after the preflight adopted the same EncodedInputStream and shared parse
flags as the DOM reader. This preserves reader compatibility while enforcing
the nesting budget.

Final verification, all successful:

```sh
cmake --build build/Release-linux --target dal_cpp_tests dal_public_tests script_perf rate_risk_perf -j 12
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptInstrumentTest.*:CalibrationProtocolTest.*:ScriptTest.TestParse*:ScriptTest.TestEvent*:ScriptPreprocessorTest.*:ScriptPastReplayTest.*:ScriptExactFoldingTest.*:ScriptCompiledParityTest.*'
./build/Release-linux/dal-public/dal_public_tests --gtest_filter='ScriptArchiveTest.*:ScriptTest.*'
cmake --build build/warning-clean --target dal_cpp dal_public -j 12
python3 .github/scripts/check_docs.py
./build/Release-linux/dal-cpp/benchmarks/script_perf/script_perf
sha256sum build/Release-linux/dal-cpp/benchmarks/rate_risk_perf/rate_risk_perf
```

Results: 171 core tests (including 34 contract/protocol tests), 15 public tests,
GCC 14.3 warning-clean core/public libraries, 79 Markdown files, frontend smoke,
modified-range formatting and diff checks pass. Nine tests are added in this
follow-up. Lizard checks of added/modified functions stay within CCN 8; older
unchanged parser functions are outside this repair's complexity scope.

Frontend smoke is informational and runs after compilation finishes. The final
rate-risk executable retains local SHA-256
`a9dfaf72ab416f8223da429447e91fa64754a46ac49ee732e1a31607b4d787c6`.
The changed loader/parser paths are absent from that executable, so the previous
paired rate-risk evidence remains applicable without another timing run. No full
test/benchmark matrix, Windows execution or downstream calibration capability is
claimed by this local verification. Required CI and the coordinator's focused
blocker recheck still apply to the pushed head.
