# DAL-401: bounded same-instrument Monte Carlo

## Controlling inputs and design

Implementation starts from clean master `bd4bf20bc2c96359509c88c588e5d40089cf1809`,
including accepted DAL-398/399/400. Research attachment
`01a15c20-3723-7ddd-8ef1-c0ac706cdc16`, sections 6 and 8.1–8.4, and the
DAL-398 API note control this change. The parent authorization and latest
DAL-400 review/repair discussion were read before implementation.

The new core wrapper owns the admitted immutable instrument and a disposable
prepared script. A narrow bound-product preparation bridge preserves typed
slots, receiver selection, history/fixing rules and the existing observation
planner. It uses the Gaussian factory, hard tree/compiled evaluators and native
AAD RecordingScope. No native option is retained and callers supply no Product.
Only the two admitted P0 templates execute. Smoothing is rejected for both
value and derivative runs. Backend and derivative capability reflect execution.

One MRG32 stream uses explicit seed and absolute normal-path offsets. Fit and
validation reserve separate contiguous ranges; batches occupy disjoint
subranges. The mapping records timeline, dimension, sequential factor-major
coordinates per advancing sample, Precise inverse normal and no Brownian bridge.
CRN reuse is per instrument/mapping; equal seeds across products prove nothing.

Price returns batch means, gradient batch means/SE and a raw one-row
quoteJacobian in requested exact native-label order. G coordinates are resolved
from original factor names and knot dates; duplicate or missing labels fail.
Affine Deposit views replace only proven payment RHSs with their AST-derived
A/B coefficients, execute separately on the same paths, and pool before taking
the quotient. Paired batch covariance and the full four-variable delta method
give value and quotient-gradient uncertainty. Engine run counts include views.

## Frozen numerical acceptance (before implementation)

The independent fixtures use USD N=100, ACT/365 concrete dates, log discount
nodes 0,-0.04,-0.08, g=0.02 and H=1. BondOption E=2027-10-21,
T=2028-10-21, K=0.97; Deposit ALPHA=1.2, K=0.025, S=2026-10-21.
Native parameter scales are 0.01 (g) and 1 (log discount).

MRG32 seed 1024; fit 8 batches of 8192 paths, validation 16 of 16384;
the fixed total budget is 327680 paths per view. Refinement compares validation
prefix halves with the full budget; no seed search or adaptive scale changes.
Price errorScale=0.1; Rate errorScale=0.001. k=3, nu=nuG=0.25,
epsilonV=1e-10, epsilonG=1e-7. GScale=errorScale/parameterScale.
Fast g FD uses h=1e-4 and h/2; eFD is their absolute difference, capped by
the same gradient precision budget. Same-path MC FD uses both steps and records
refinement separately; price parity does not establish gradient parity.

Acceptance implements every research 8.3 value condition, gradient parity and
precision, and denominator confidence for affine measures. Numerical floors
must be below the frozen budgets. Budget failure remains an explicit failed
result, with no scale enlargement. Independent discount/Black mathematics,
fast descriptors and original-source script MC are compared separately.

## Scope and verification plan

RED/GREEN cycles cover execution/binding parity, path-range reproducibility,
exact-label failures, tape cleanup, affine covariance/quotient uncertainty and
budget failure. Nearby preparation, history, compiled and Gaussian tests cover
the bound-product bridge. No example/build topology changes are planned.
Performance scope is the new Deposit/option tree/compiled/AAD caller and its
prepare/evaluate cost. Existing algorithm benchmarks are repeated only if
changed caller provenance requires them. Dependency pins and C++17/native AAD
configuration remain unchanged.

## Implementation choices

The core API is a prepare/evaluate pair; later facade request/language mappings
remain reserved for DAL-407. The internal bound-input adapter preserves the
typed AST and existing observation/history pipeline without changing legacy
MCSimulation's fixed-seed driver. Existing data preparation executes the same
statements and validation order. IRN remains Price/None admission-only;
executable extensions require MRG32. No solver, public binding, example,
union timeline or Sobol/RQMC implementation is added.

A recording prefix is shared within a view; each path clears adjoints and
performs its own reverse sweep. engineRuns counts logical batch/view executions,
with model initialization amortized across batches. Price validation has 16
batch/view executions over 262144 paths. ParRate has 32 over 524288 actual paths
(262144 per A/B view). Fit has 65536 paths per view. Non-scalar caller tape mode,
nested recording and zero-g derivative boundaries fail explicitly. Caller mode
and active outer recordings remain usable; exceptional path overflow cleans up.

## RED and GREEN

The first missing-wrapper test failed to compile because calibrationmc.hpp did
not exist. Once the first Release library and gtest archives were available,
a focused standalone executable established the first GREEN while the ordinary
test target built. Subsequent cycles use dal_cpp_tests.

- Wrapper GREEN: original-bound tree and compiled path payoffs match. Early
  adapter configuration failures (legacy positive smooth setting and
  case-insensitive DSL receiver spelling) were corrected without changing tests.
- AAD RED: TestAad* observed zero gradient columns; TestMissing* accepted invalid
  labels. GREEN: exact native columns, path gradients, raw row Jacobian and
  CRN FD(h,h/2) agree without another path normalization.
- Affine RED: coefficient statistics were absent. GREEN: synthetic paired data
  distinguish pooled quotient from mean ratios, test A/B covariance and all
  four quotient-gradient influence terms, and reject denominator instability.
  Deterministic original-definition views match independent discount mathematics.
- Validation RED: precision/report types were absent. GREEN: the small budget
  returns NumericalValidationFailed with owned Validate report; reused fit
  streams and numerical floors as large as the budget are rejected.
- Edge RED: zero-g option AAD was admitted. GREEN: the interior/nondegenerate
  guard rejects it while double Price remains available.
- Tape RED: multi-adjoint mode was accepted. GREEN: owned DerivativeUnavailable
  at Evaluate preserves the caller width/mode. Nested recording, path-overflow
  cleanup, repeated evaluations and subsequent ordinary recording also pass.
- Confidence RED: synthetic affine statistics admitted a higher validation
  sigma without rechecking the denominator. GREEN: both fit and validation
  recheck their owned annuity direction/minimum at the requested sigma. This
  synthetic gate test does not change the frozen numerical fixture scales.

Commands for the focused cycles, after their builds:

```bash
cmake --build build/Release-linux --target dal_cpp_tests -j 8
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='CalibrationMcTest.TestSameBindingTreeAndCompiledPathPayoffs'
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='CalibrationMcTest.TestAad*:CalibrationMcTest.TestMissing*'
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='CalibrationMcTest.*:CalibrationMcNumericalTest.*:CalibrationPreparationTest.*'
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='CalibrationMcTest.TestMultiAdjointModeIsRejectedWithoutChangingCallerMode'
```

No assertion or statistical tolerance was weakened. A temporary missing climits
test include was corrected before the boundary test ran. Helpers were split
while green; changed/new production and test functions all have CCN <=8.
Existing IRN admission regression tests remain unchanged.

## Frozen numerical results

Every research 8.3 value condition and selected gradient parity/precision
condition passes. Independent Black mathematics and the native Gaussian fast
pricer agree at the center and four FD bumps. Native fast g FD is 33.1327643979
at h and 33.1328294122 at h/2; eFD=0.0000650143 is below the frozen gradient
budget. Independent closed-form vega verifies FD separately. E-to-T uses the
actual 366 days. Oracle/fast/MC comparisons are explicit; no arbitrary-script
numerical certification is implied.

Validation mean, SE and native g mean/SE:

- Bond call Price: 0.3818199532, 0.0018352207; g 33.13984964 / 0.13088570.
- Bond put Price: 1.2631106816, 0.0022349690; g 32.96264200 / 0.08375639.
- Deposit Price: -1.0366847073, 0.0019409723; g 0.09945514 / 0.09738850.
- Deposit ParRate: 0.0339914231, 0.0000170109; g -0.00087164 / 0.00085349.

Deposit log-discount partials are 98.9633152927 (SE 0.0019409723) for Price
and -0.8673247564 (SE 0.0000170109) for ParRate, compared with independent
103*exp(-0.04) and -exp(0.04)/1.2. Option statistical acceptance selects g;
the additional log-discount column has separate pathwise tree/compiled/FD evidence.

Validation-prefix refinement uses eight rather than sixteen batches, with
unchanged seed, fit reservation, batch paths, scales and maximum budget. Both
resolutions pass the same gates. Absolute full/prefix changes are 0.0023127396
for call Price, 0.0029021371 for Deposit Price and 0.0000254339 for ParRate.
The last is a ratio-bias indicator, not proof of unbiasedness. AAD also passes
same-path CRN FD(h,h/2), including pooled affine derivatives.

## Final verification and provenance

GCC 15.2, C++17 Release, native AAD, Eigen enabled, native architecture disabled,
static libraries, public enabled. Dependency pins are unchanged: Eigen 3147391d,
gtest d72f9c8a, Machinist e76b0ef2, pybind11 8a099e44, rapidjson ea152ded.
At final fetch origin/master remains bd4bf20bc2c96359509c88c588e5d40089cf1809.

```bash
cmake --preset Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/Release-linux --target dal_cpp_tests dal_public_tests -j 8
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='CalibrationMcTest.*:CalibrationMcNumericalTest.*:CalibrationPreparationTest.*:ScriptInstrumentTest.*:CalibrationProtocolTest.*:ScriptPastReplayTest.*:ScriptExactFoldingTest.*:ScriptCompiledParityTest.*:ScriptFixingPreparationTest.*:GSREuropeanTest.*:GSRMultiFactorTest.*'
build/Release-linux/dal-public/dal_public_tests --gtest_filter='ScriptArchiveTest.*:ScriptTest.*'
python3 .github/scripts/check_docs.py
git diff --check
```

Passed: 205 core tests (20 new MC), 15 public compatibility tests, 81 Markdown
documents, clang-format for new files/modified ranges, diff checks and lizard
CCN <=8. GCC 14.3 syntax-only compilation of the four production translation
units passes exact Linux CI warning categories/exclusions with Werror, Eigen
and C++17. This is not a second complete GCC 14 build. Enum markup is unchanged.

Final SHA-256 identities:

- dal_cpp_tests: 576a12ba59ec447c1c7e8c2c7a99495478becc11cdfbde11374d5f31b76d10dd
- dal_public_tests: 19510532daee3a5a93b47fc8a505032333d6f8bff6c2d052b4cbc3e787dd90d7
- libdal_cpp.a: 68ade2fef20c25c9c97f7497b80bf73d0751d0d7809ff25143a2b18fe0da926d
- performance probe: e478ad231b8ca099a65edd7c32695ce5618c0b9cfefed055207a15a543157e50

## Performance selection and limits

The new caller is the Deposit/option bridge; bound preparation is covered by
history/fixing, compiled and public compatibility tests. Legacy preparation's
statement sequence, model/RNG/numerical kernels and solver callers are unchanged.
No calibrated regression-gate executable's algorithm changes. Unrelated curve,
XCCY, SLV, solver and tape-kernel matrices are excluded. No example/build topology
changed, so public-disabled, standalone and example build matrices are outside
the changed callers; omitted cases are not reported as passes.

The attached probe uses the committed fixtures and Release archive on CPU 0 of
an i9-13900HX. After one warmup round, ten rounds per case time 100 prepares and
three evaluations of the fixed 65536-path fit schedule, with observable checksum.
It covers both products, tree/compiled and double/AAD. This is a new informational
baseline, not a paired regression verdict or a production adequacy promise.

Across cases, preparation ranges from 121.76 to 172.94 us; fit evaluation from
5.82 to 6.88 ms for double and 23.22 to 26.00 ms for AAD. The raw ten-sample CSV
is retained instead of interpreting noise as speedup. Probe max RSS is 11112 KiB.
Compiled AAD peaks are 79 nodes for Deposit Price and 87 for call, with 1966080
retained tape-capacity bytes. The full 205-test selection's RSS is 95348 KiB,
including unrelated test harness/history storage.

Raw RED/GREEN logs, numerical XML, probe source/CSV and resource output accompany
the issue handoff. Local Windows/MSVC, full product tests and full benchmarks
were not run. Current-head CI and focused reviewer acceptance remain with the
coordinator; this task neither merges nor activates downstream tasks.

## Windows budget-conversion repair

Input head: `eb472e47d665dc695c2543d604cc924d4364f718`. Actual RED evidence is
Windows run `39575555999`, main job `119140142054` and diagnostics job
`119140142146`: both report C4244 for the implicit size_t-to-double argument
conversion when encoding the plan's budget. This is a code diagnostic, not an
external permission/service failure. No gate, warning policy or numerical
acceptance condition is changed.

The production repair is one expression: make the conversion to double explicit
at budget serialization. This is a conversion of an already validated value:
PrepareCalibrationMc calls ValidateSettings/ValidateSampling before building
the identity, and that existing guard requires maxPathsPerView <= INT_MAX.
On the supported Windows/Linux types, INT_MAX = 2^31-1 is below double's exact
integer limit 2^53. The existing bound is retained, not extended to larger
representable integers. Values above it, including nonrepresentable integers,
fail before fingerprinting; there is no rounding, clipping or lossy admission.
The JSON number, plan identity inputs and actual fit/validation counts retain
their previous definitions.

Two new characterization tests pass before and after the expression repair.
They check INT_MAX-1 and INT_MAX for distinct/reproducible identities, unchanged
mapping and bitwise-equal actual fit/validation path values and counts.
INT_MAX+1, SIZE_MAX and (on 64-bit size_t) 2^53+1 retain owned Prepare-stage
InvalidCalibrationPlan reports at /mc/max_paths_per_view after input unwind.
The tests do not execute a huge path schedule: changing the maximum budget does
not change the small explicit schedule. Four existing sampling/range/oracle
tests complete the directly affected selection. No runtime test failure is
claimed for this compile-only defect; RED is the actual MSVC compiler failure.

Local verification builds only the core and a focused executable from the
committed test source and repository test main, using the Release core/gtest
archives. This avoids rebuilding or rerunning unrelated test cases:

```bash
cmake --build build/Release-linux --target dal_cpp -j 12
g++-15 -O3 -DNDEBUG -std=c++17 -fPIE -ffp-contract=fast -I dal-cpp -I dal-cpp/externals/rapidjson/include -isystem dal-cpp/externals/googletest/googletest/include -c dal-cpp/tests/script/test_calibration_mc.cpp -o build/Release-linux/dal401_budget_test.o
g++-15 -O3 -DNDEBUG -std=c++17 -fPIE -ffp-contract=fast -I dal-cpp -isystem dal-cpp/externals/googletest/googletest/include -c dal-cpp/tests/test_main.cpp -o build/Release-linux/dal401_budget_main.o
g++-15 -O3 -DNDEBUG -ffp-contract=fast build/Release-linux/dal401_budget_main.o build/Release-linux/dal401_budget_test.o build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o build/Release-linux/dal401_budget_tests
./build/Release-linux/dal401_budget_tests --gtest_filter='CalibrationMcTest.TestBudgetIdentityRetainsExactAcceptedBoundary:CalibrationMcTest.TestOversizedBudgetsKeepOwnedPreparationFailure:CalibrationMcTest.TestSamplingRequiresOptInBatchesSeedAndBudget:CalibrationMcTest.TestZeroSeedAndExplicitCountBoundaries:CalibrationMcTest.TestRangesMatchSequentialNormalsAndIndependentPathOracle:CalibrationMcTest.TestSameBindingTreeAndCompiledPathPayoffs'
```

All six selected tests pass on both input and repaired libraries. Gtest XML
records the two upper-bound identities; comparing the before/after properties
confirms exact equality. GCC 14.3 syntax-only compilation of calibrationmc.cpp
with the existing CI warning categories/exclusions and Werror passes. Modified
range formatting, CCN <=8 and diff checks pass. The original statistical scales,
sample counts, error budgets, mappings, numerical kernels and dependencies are
unchanged.

With identical local GCC 15.2 Release configuration, the rebuilt files retain
their input-head SHA-256 exactly:

- calibrationmc.cpp.o: `1dcda78e90f176c3f8cf5b02274e75044a2a0e84502b948f993af574f654258e`
- libdal_cpp.a: `5c37f4562f10456a00d66123e8f0945322ade64a178c6322dce1810f92ea1cd2`
- focused test executable: `10bf9762e1fc7e6b2cf7fabb1c0bc3559b60a1707a62ab2393163b0b49d5359a`

These are local before/after identities, not claims of equality with the
archived binaries built at other workspace paths. Together with the unchanged
runtime source/dependency/configuration provenance, they support reusing the
original 205-core/15-public, frozen statistics and informational performance
evidence in attachment `01a15dc0-6f4a-7d09-ad68-993d19c18c08`. No full test or
benchmark matrix is repeated. Local MSVC execution is not claimed; the pushed
head's real Windows checks and coordinator-arranged review remain required.
