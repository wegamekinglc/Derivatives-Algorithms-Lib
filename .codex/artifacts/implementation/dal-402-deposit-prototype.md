# DAL-402: Deposit same-source prototype

## Inputs and design frozen before implementation

Clean startup HEAD/master is `15a3b445362062e75099de3bf9b4353fa2225873`,
including accepted DAL-398–401. The parent authorization/scheduling thread,
DAL-398 API note (especially sections 3, 4, 10), upstream implementation
records, methodology and latest research attachment
`01a15c20-3723-7ddd-8ef1-c0ac706cdc16` sections 3.2, 7.1, 8.1–8.4, 10
control this work. The research incorporates the expert critiques; no new
schema or model design is proposed.

Only new Deposit-owned curve files and tests change. A native Deposit is read
once: use its existing single-period adjustment/day-count routine to emit
N/K/ALPHA and two concrete PAYS. The input has no notional/direction; explicit
positive import N (default one) creates the lender's principal outflow and
redemption inflow. K is the imported MarketRate, never a live quote.

A prepared curve adapter owns the shared Price/ParRate preparation and its
AST-derived descriptor. It caches only fixed-layout ACT/365F log-linear
interpolation weights. Candidates supply logDF nodes, with an anchored zero;
no schedule/text/native product is rebuilt during iteration. Candidate identity
includes the base plan and complete curve content. Only date/currency/layout
matched discount-only snapshots enter; model construction and MC use the
existing shared bridge separately. A one-free-node bracketed Brent adapter
uses a shared ParRate observation and its frozen Rate scale. No general curve
provider, approximate objective or MC optimizer is introduced.

## Frozen numerical and performance selection

Independent oracle terms: V=S=2026-10-21, T=2027-10-21, ACT/360,
N=100, K=.025, alpha=365/360. Direct cashflows are -100 at V and
100*(1+.025*365/360) at T. Native theta=log P(T). Oracle Price,
A/B and ParRate, plus their derivatives, are elementary exponentials of
these original terms, independent of exporter/lowerer/interpolator.
Initial theta=-.04, nonroot=-.06, fit=-log(1+.035*alpha), valid bumps
theta_fit +/- 1e-4 and +/- 5e-5. Deterministic value tolerance 1e-10;
analytic/AAD tolerance 1e-9; central FD h=1e-4 and h/2, tolerance 1e-6.

MC freezes MRG32 seed 1024, g=.02, H=1, fit 8*8192 and validation
16*16384 normal paths, max 327680 per view, compiled AAD. Columns are
logdf:OIS:2027-10-21 and g:2026-10-21. Price scale=.1 amount,
ParRate scale=.001 rate; sigma=3, nu=nuG=.25, epsilonV=1e-10,
epsilonG=1e-7, native parameter scales={1,.01}. Reference errors are
zero for the elementary formula (epsilon handles rounding). All three
oracle/fast/MC edges must pass the shared value/gradient indicators;
half-validation uses eight batches with the same reservation, paths and
thresholds. Small-budget and unstable-annuity counterexamples must fail.
Zero-g checks are supplementary in the same log-linear/time/route domain.

New callers are exporter, prepared candidate residual/Jacobian and scalar
Brent. Measure separate preparation, evaluation and fitting information
baselines in ten rounds after warmup with observable checksum. Legacy native
and script/model kernels are untouched; focused legacy tests cover linkage,
and upstream statistical/stream tests remain applicable as source evidence.
No benchmark regression gate algorithm or example/build topology changes;
unrelated matrices and new public-disabled/standalone builds are excluded.

## Delivered behavior and RED/GREEN

`dal-cpp/dal/curve/depositscript.hpp` and `.cpp` add import settings,
`NewDepositScriptInstrument`, `PreparedDepositCurve_` and
`FitDepositCurveNode`. The shared protocol, preparation, MC, model, existing
solver and native instrument implementations are unchanged. All new tests
are in `dal-cpp/tests/curve/test_deposit_script.cpp`.

The scalar solver uses existing `BracketedBrent_`, one non-anchor coordinate,
the observation's frozen Rate scale and strict abs(scaled residual)<1.
Failures retain observation/quote axes, code, evaluation count, raw/scaled
residual and last validated curve. Preparation and candidate evaluations are
separate: candidates refresh log-DF values and analytic partials without
reparsing source. Changes to definition/binding need another adapter; old
objects remain usable. Unsupported routes/layouts/conventions never fall
back to native pricing or MC.

Focused RED/GREEN commands and raw logs are in the handoff evidence bundle:

1. `g++-15 -std=c++17 -fsyntax-only -I dal-cpp -I dal-cpp/externals/googletest/googletest/include dal-cpp/tests/curve/test_deposit_script.cpp`
   failed because depositscript.hpp did not exist (`red-import.log`). After
   minimum import implementation, TestImportFreezesConcreteLenderCashflows
   passed (`green-import.log`). The first runtime GREEN used a focused
   executable linked against this checkout's core/gtest archives.
2. `bash evidence/run-focused.sh --gtest_filter='DepositScriptTest.TestCandidateUsesSourceAlphaAndIndependentCashflowDerivatives'`
   failed to compile without PreparedDepositCurve_ (`red-candidate.log`).
   Its implementation passed the independent off-root value/gradient oracle.
   The separate projection/spot/collateral test then failed at runtime because
   import accepted a projection curve (`red-import-domain.log`). Import guards
   made all three tests pass (`green-candidate-domain.log`). A test setup typo
   used nonexistent collateral LIBOR; it was corrected to real GC before
   reaching that subcase, without changing the required rejection.
3. The focused solver test failed to compile without FitDepositCurveNode
   (`red-solver.log`). The minimum Brent adapter passed all four tests
   (`green-solver.log`). A Handle conversion compile error was corrected
   before runtime verification; it is not claimed as a behavioral RED.
4. Invalid notional had the source-row path instead of /import/notional
   (`red-import-input.log`). The focused input/AAD selection passed after
   the guard (`green-input-aad.log`). Initial AAD test code was corrected
   to the actual RecordingScope/free-adjoint APIs before this behavioral RED.
5. An unset currency leaked std::logic_error constructing String(nullptr)
   (`red-unset-currency.log`); explicit token validation made the owned-error
   and native-input tests green (`green-import-guards.log`).
6. An extreme but finite source with N=1e300, ALPHA=1e8, K=0 and theta=-710
   has a finite affine quotient but overflows the explicit 1/DF kernel.
   TestFastKernelCannotReturnNonfiniteRateForFiniteAffineValue first returned
   infinity without throwing (`red-kernel-overflow.log`). The adapter now
   rejects it with CalibrationNumericalFailure; final core verification is
   GREEN. No stable general-kernel extension or silent fallback is claimed.

Characterization/numerical tests additionally cover native adjusted maturity,
rejected adjusted start, exact routes/layouts, multiple interpolation weights,
independent native AAD, fee/direction mismatch, source-only N/K/ALPHA changes,
zero-g tree/compiled checks in the matching domain, explicit QuoteCopy and
stale MC plan rejection, CRN FD(h,h/2), raw Jacobian normalization, owned
solver-budget failures, ratio covariance and unstable denominators. No test
or frozen statistical condition was weakened. Green refactoring split solver
validation/evaluation and the CRN helper; all new functions have CCN <=8.

## Numerical results

The one-node fit takes six evaluations and returns theta=-0.034870989026579077.
The fixed 2.5% coupon has PV=-0.97914291460338632 at the 3.5% fitted par rate.
Only explicit K=q creates a new binding with zero PV; the original fast and
MC plans retain their old contract and reject the new binding when supplied
to an old shared plan.

All seven states pass OracleFast, OracleMC and FastMC for values and selected
native derivatives. The independent oracle uses original fixed cashflows;
g derivative is zero in expectation, not a claim that each path has zero
g sensitivity. Fit/validation and the eight-batch refinement pass the same
frozen gates. The XML records complete definition/binding, candidate,
context, mapping and plan identities plus raw values/SE/gradients and engine
counts. Selected validation results (mean / SE) are:

- Initial Price: -1.4837277308272354 / .001932204460471699;
  oracle -1.4857217424718669. Log-DF derivative 98.516272269172831 /
  .001932204460452515; g .099005875727842765 / .096948571685108953.
- Initial ParRate: .040230944615825041 / .000020133428429312167;
  oracle .040251722491122618. Log-DF derivative -1.026532314478837 /
  .000020133428429032924; g -.0010316339465242718 / .0010101565232486986.
- Fitted Price: -.97713864940166784 / .0019421402168718484;
  oracle -.97914291462679159. Price total error indicator .007830685875739303
  is below the frozen .025 budget.
- Fitted ParRate: .034979328421820256 / .000020030428224354413;
  oracle .035. Rate total error indicator .00008076286285280332 is below
  the frozen .00025 budget.

Initial full/prefix differences are .002889027347923845 amount and
.00003010256980497117 rate. The rate test independently reconstructs its
delta-method SE from the returned paired B/A covariance, including its
nonzero cross term, and checks denominator confidence. These are statistical
indicators, not strict probability or ratio-bias bounds. Price validation
executes 16 batch/view runs; ParRate executes 32. Peak AAD nodes are 78/71,
with 1966080 retained tape bytes. Small precision budgets fail rather than
changing scale, and a deliberately unstable denominator is rejected.

## Final verification and provenance

GCC 15.2.0, C++17 Release, static libraries, native AAD, Eigen ON,
native architecture OFF, public ON, examples/benchmarks OFF. Pinned submodules
are unchanged: Eigen 3147391d, gtest d72f9c8a, Machinist e76b0ef2,
pybind11 8a099e44, rapidjson ea152ded. Final fetched origin/master remains
the startup 15a3b445 commit; the isolated task branch is
`agent/dal-implementer/2a3458a7c4ec`. No user changes were present at startup.

```bash
git submodule update --init --recursive
cmake --preset Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/Release-linux --target dal_cpp_tests dal_public_tests -j 10
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='DepositScriptTest.*:DepositScriptNumericalTest.*:YCInstrumentTest.*:CalibrationTest.*:LogDfInterpolationTest.*:RootFinderTest.*:CalibrationPreparationTest.*:CalibrationMcTest.*:ScriptInstrumentTest.*:CalibrationProtocolTest.*' --gtest_output=xml:evidence/final-core.xml
build/Release-linux/dal-public/dal_public_tests --gtest_filter='ScriptArchiveTest.*:ScriptTest.*'
python3 .github/scripts/check_docs.py
git diff --check
```

Passed: 139 selected core tests, including 16 new Deposit tests; 15 public
compatibility tests; documentation checks (82 Markdown files); formatting,
diff checks and CCN <=8. GCC 14.3 syntax-only compilation of depositscript.cpp
passes the exact Linux CI warning categories/exclusions with Werror, C++17
and Eigen include/definition. Full command is in warnings-command.txt.
This does not claim a complete GCC 14 build or local Windows/MSVC execution.
Enum markup, build configuration and all shared/model files are unchanged.

Final executable SHA-256 values:

- Core archive: `2c6b15e354990f8992ea76b74a92f83c1b49ed74d5eb8676094cf788579e5ec5`.
- Core tests: `b9da51143f9328ab6fd4ef7f9fb93e327d95921333c7729e1e7a595a7c4fe540`.
- Public tests: `1b9a75bb61d8e51e85c628a0754d8864b7958c584d34383b33fbc46cbbbbc9b9`.
- Timing probe: `a5c0d80fbeeb5d4d63ccef007f2fdc6801b34611f720a92ec29e6fb17fb9f32e`.

## Performance scope and limits

The new probe uses the final core archive, CPU 0 of i9-13900HX, one warmup
round and ten recorded rounds per layout. Each round calls import 100 times,
prepare plus one evaluation 100 times, cached evaluation 10000 times and
one-node fitting 100 times, retaining an observable checksum. Two-node and
three-node layouts cover exact maturity and maturity between nodes. Median
microseconds per operation, respectively, are import 109.31/98.69,
prepare-plus-evaluate 137.89/111.12, cached evaluation 6.91/7.78,
fit 52.90/45.16. Raw samples and source accompany the handoff. These are
new informational baselines, not paired speedup or regression-gate verdicts.
Probe RSS is 7672 KiB; the 139-test selection RSS is 37504 KiB.

New cached residual/Jacobian and scalar-solver callers are measured. No legacy
kernel or calibrated benchmark caller changes, so unrelated benchmark matrices
are excluded. Upstream MC stream/statistics/model evidence is reused only as
unchanged-source evidence; its old executable identities are not presented as
this build's results. Directly relevant preparation/MC and public compatibility
tests were rebuilt and run above. No examples or build topology changed;
standalone/public-disabled/example matrices and full regressions were not run.
No general curve provider, MC optimizer, facade/bindings, XCCY or quote-risk
capability is asserted. There is no shared-API blocker or scope deviation;
focused reviewer acceptance and actual current-head CI remain with coordinator.
