# DAL-403 Gaussian BondOption prototype

## Controlling evidence and scope

Start: clean branch `agent/dal-implementer/4a63910df848` at
`15a3b445362062e75099de3bf9b4353fa2225873` (merged DAL-401 PR #609).
DAL-398 API contract, DAL-399/400/401 implementation records and
`docs/methodology/script_calibration_contract.md` control this work. Research
attachment `01a15c20-3723-7ddd-8ef1-c0ac706cdc16`, sections 3.2, 7.3, 8 and 10,
was downloaded through Multica. Its expert critiques and frozen API decisions
are inputs, not a request to reopen accepted upstream work.

Exclusive changes: model-owned BondOption importer, fast executor and selected-g
adapter, plus dedicated model tests. Shared definition/preparation/MC/protocol,
curve code, build lists and existing entry points remain read-only. No example
or facade changes are planned. Shared guide/CHANGELOG/index integration belongs
to DAL-402 now; drafts will be retained here for serial integration before merge.

## Design frozen before implementation

`NewBondOptionScriptInstrument` imports native expiry/maturity/K/direction once,
with explicit currency, N and route, into the existing immutable source/header.
The native input is discarded. Preparation reuses the complete P0 AST matcher;
each fast evaluation constructs an ephemeral BondOption from that descriptor,
uses the identical Gaussian model and multiplies source N exactly once.
Native one-factor and multi-factor Gaussian Price are supported; selected-g
calibration uses MultiFactorGSRModelData and existing bounded Gauss-Newton.
No retained CalibrationQuote/native option is a pricing authority.

Fast evaluation and calibration expose actual FD native partials, identities,
raw/scaled residuals, stable observation axes and actual price-call counts.
FD steps are explicit native-g steps for standalone evaluation; solver FD retains
the existing normalized-coordinate relative-step convention. Bounds, prior and
smoothing retain existing GSR meanings. Candidate contexts are rebuilt rather
than reusing plans after model changes. Hard payoff and the native GSR numeraire
are required; smoothing/model/curve/binding changes invalidate preparation.
Degenerate distributions allow double Price, but central g partials at zero g
or a degenerate payoff are unavailable. Bound solver differences are explicitly
one-sided where needed, not labeled analytic/AAD.

## Frozen validation before implementation

Main fixture: USD OIS, V=2026-10-21, E=2027-10-21, T=2028-10-21,
log discounts (0,-0.04,-0.08), N=100, K=0.97, constant g=0.02,
constant H=1 and one named factor level. E-to-T is 366/365, E-to-V=1.
Independent normal-CDF mathematics gives sigma=g*(366/365), forward=D(T)/D(E),
PV=N*direction*(D(T)*Phi(direction*d1)-K*D(E)*Phi(direction*d2));
vega=N*D(T)*phi(d1)*(366/365). No library Black/forward substitution is the oracle.

At initial g=0.012, non-root g=0.016, fitted g and h/h2 bumps, compare independent
math, script fast and same-binding MC. h=1e-4, h/2=5e-5 for fast refinement;
CRN h=1e-5 and 5e-6. Fast value tolerance 1e-10; vega FD absolute 2e-4.
MRG32 seed 1024, Precise normals, no bridge; fit 8*8192, validation 16*16384,
maximum 327680 paths/view, disjoint normal-path intervals. sigma=3, nu=nuG=0.25,
Price scale=0.1, native parameter scale=0.01, epsilonV=1e-10,
epsilonG=1e-8. These values cannot be relaxed after observing results.
Use ValidateCalibrationMc for value/gradient parity and precision; report budget
failure independently from optimizer convergence. A smaller validation prefix
checks refinement without changing thresholds; an insufficient budget must fail.

Dedicated counterexamples: fee/second payment, fixing-dependent strike, delayed
settlement, PUT/source changes, model switch to SLV, curve/context mismatch,
smoothing and incompatible cached bindings. Same scenario tests compare tree
and compiled PAYS including numeraire and explicit delayed-payment semantics.
Zero g, boundary, at-the-money degeneracy and beyond-curve dates are explicit.
Cost evidence separates preparation, fast residual/Jacobian evaluations, MC
engine runs, path counts and tape. New APIs receive an informational baseline;
unchanged legacy GSR calibration gets directed correctness evidence. No full
benchmark/statistics matrix or unrelated SLV/curve tests are intended.

## Execution evidence

The importer and evaluator are in `dal-cpp/dal/model/gsrbondoption.hpp` and
`gsrbondoption.cpp`. The selected-g adapter is in `gsrbondoptioncalibration.cpp`;
dedicated tests are in `dal-cpp/tests/model/test_gsr_bondoption_script.cpp`.
These are additive core APIs. Existing native entry points, source files,
protocol enums, archives and build lists have no changes.

### RED, GREEN and refactor

The first focused RED used:

```sh
g++-15 -std=c++17 -fsyntax-only -I dal-cpp -isystem dal-cpp/externals/googletest/googletest/include dal-cpp/tests/model/test_gsr_bondoption_script.cpp
```

It failed because `dal/model/gsrbondoption.hpp` was absent. After implementation,
the exporter ownership/rebind/archive test passed. The next focused compile
failed on missing PriceGaussianBondOption; its implementation made both exporter
and independent call/put price tests pass. The calibration test then failed on
missing CalibrateGaussianBondOptions.

Runtime cycles used the focused test executable linked against the freshly built
Release core and pinned gtest archives:

```sh
cmake --build build/Release-linux --target dal_cpp -j 10
g++-15 -O3 -DNDEBUG -std=c++17 -I dal-cpp -isystem dal-cpp/externals/googletest/googletest/include dal-cpp/tests/test_main.cpp dal-cpp/tests/model/test_gsr_bondoption_script.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o build/Release-linux/dal403_tests
build/Release-linux/dal403_tests --gtest_filter='GSRBondOptionScriptTest.*'
```

- Calibration initially returned StepStalled at g=0.020000000000000108,
  residual=-9.1593399531575415e-15. The new adapter now rechecks the **same**
  projected-gradient criterion at that accepted final point. It reports
  ProjectedGradient only when that criterion passes, including penalty rows.
  Existing solver code and its legacy stopping behavior are unchanged.
- Negative Price targets and an empty observation vector initially solved
  without rejection. Domain admission now rejects both before fitting, with
  owned observation axes in the error report.
- A focused compile rejected the missing nativeJacobianAvailable field.
  Zero selected g and degenerate option distributions now suppress the raw
  native Jacobian. Solver bound differences remain available for search and
  oneSidedColumns explicitly identifies truncated stencils.
- An extreme scale (1e-300) initially leaked the native NormSquared overflow
  exception. It now becomes an owned Solve-stage CalibrationNumericalFailure,
  retaining observation axes and the original diagnostic text.
- A final API-contract check caught a missing owning result report (compile RED)
  and implicit unit notional (runtime RED). Results now own the existing common
  axes/error report, while numeric metrics remain on the domain result. An omitted
  import notional is invalid rather than silently becoming N=1.
- Existing complete-structure/model guards passed their new characterization
  tests immediately; they are reused evidence, not claimed new runtime REDs.

Helpers were extracted while green to keep production/test functions at CCN<=8.
Two test setup errors were corrected: the general script preparation API requires
its legacy positive smoothing setting even for double hard evaluation; the
MC reference requires an explicit zero gradient-SE vector for fast FD. Neither
correction changes payoff assertions, sample counts, floors or acceptance bars.

### Numerical results and limits

All six frozen call/put comparisons at g=0.012, 0.016 and 0.02 pass independent
oracle/fast values, vega FD and full MC value/gradient parity and precision.
At g=0.02 the validation call mean/SE are 0.3818199532/0.0018352207 and
native-g gradient/SE 33.13984964/0.13088570. Put mean/SE are
1.2631106816/0.0022349690 and gradient/SE 32.96264200/0.08375639.
Fast FD(h)=33.1327643979, FD(h/2)=33.1328294122; eFD=0.0000650143.
The same fixed contract survives fast fitting and MC/AAD repricing. Its
eight-versus-sixteen validation-batch prefix changes call PV by 0.0023127396;
both pass unchanged gates. The 48-path small schedule fails numerical validation
with budgetExhausted, rather than relaxing its scale.

The one-row end-to-end fit makes 20 fast kernel calls, 20 preparations and 20
candidate ModelData constructions, with zero MC paths in fitting. Validation
executes 16 engine runs and 262144 paths; its mapping reserves the earlier
65536 fit-path positions without executing them in this fast-fit example.
Compiled AAD peaks at 87 tape nodes and 1966080 retained tape-capacity bytes.
The budget counts both reserved ranges. PAYS scenario tests use the identical
explicit scenario with independently specified observation, numeraire and delayed
discount; the delayed program agrees with script semantics but is refused by
the fast AtEvent matcher.

Calibration preserves frozen bindings, initial unselected g, factor names,
g/H knots, H, correlations, curve and observation axes. Prior and smoothing
match the existing native GSR objective after explicit amount/scale conversion;
penalty rows are excluded from the raw data Jacobian. Artificial positive
parameter bounds can use a truncated FD stencil; zero-g/nondifferentiable and
degenerate distributions do not publish a two-sided native Jacobian.
The inherited numericalValidationPassed flag covers the fast backend's reported
numerical indicator only. Independent MC validation is a separate explicit
ValidateCalibrationMc result; neither zero Black error nor a passing sampling
indicator is a strict floating-point/probability bound or calibrated quote risk.

### Performance selection

Only new model-owned import/Price/calibration callers change. The dedicated
informational probe covers prepare and compatible fast Price, one/two observation
selected-g fitting, and same-instrument tree/compiled double/AAD fit evaluation.
It uses the fixed 65536-path fit schedule, one warmup round and ten measured
rounds per case. Prepare uses 100 repetitions, fast Price 100, calibration 10
and MC evaluation three per round, with an observable checksum and CPU affinity.
Raw CSV and probe source accompany the handoff.

No existing performance-gate algorithm, legacy GSR caller, shared header or
script kernel changes. Directed legacy GSR and public compatibility tests cover
the unchanged APIs; no old benchmark regression verdict or speedup is claimed.
Curve/XCCY/SLV, quote risk, full statistical/benchmark matrices and example
topology builds are outside changed callers. There are no example/build edits,
so public-disabled/standalone/example evidence is not newly required.

### Final verification and provenance

Release uses GCC 15.2/C++17, native AAD, Eigen, static libraries, public enabled,
native architecture disabled and portable Excel disabled. Dependency pins are
unchanged: Eigen 3147391d, gtest d72f9c8a, Machinist e76b0ef2,
pybind11 8a099e44, rapidjson ea152ded. No enum markup changed.

```sh
cmake --preset Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/Release-linux --target dal_cpp_tests dal_public_tests -j 10
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='GSRBondOptionScriptTest.*:GSRBondOptionNumericalTest.*:CalibrationPreparationTest.*:CalibrationMcTest.*:CalibrationMcNumericalTest.*:GSREuropeanTest.*:GSRCalibrationTest.*:GSRMultiFactorTest.*'
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptInstrumentTest.*:CalibrationProtocolTest.*'
build/Release-linux/dal-public/dal_public_tests --gtest_filter='ScriptArchiveTest.*:ScriptTest.*:PublicGSREuropeanTest.*'
python3 .github/scripts/check_docs.py
lizard -l cpp -C 8 -w dal-cpp/dal/model/gsrbondoption.cpp dal-cpp/dal/model/gsrbondoptioncalibration.cpp dal-cpp/tests/model/test_gsr_bondoption_script.cpp
git diff --check
```

Passed: 135 directed core tests (96 model/preparation/MC plus 39 contract/protocol),
including 21 new tests; 16 public compatibility tests; 82 Markdown documents;
formatting and CCN<=8. The overflow RED now passes in the ordinary core target.
GCC 14.3 syntax-only compilation of both new production translation units passes
the existing Linux CI Wall/Wextra/Wpedantic/Werror categories and exclusions,
with C++17 and Eigen. This is not a second complete GCC 14 build or local MSVC run.

Final binary SHA-256:

- Core archive: `40f1cdc393ce7788c33d8f2bc523dbf0ec3df7eaec70897099a3f876ad6c0d4c`.
- Core tests: `746d58b29ed3412aa8be4b990cbaedb8b63c316e14f8acca1282a2e5231df2e0`.
- Public tests: `16f30ace896aace156edbe9310056a9977d439c27d338a3d05bbfcd240be40e7`.
- Performance probe: `298b4e50a9864e2650a5ddb22c83135d01feab499da7baeb7b8e025e1b4be0d5`.

The probe was compiled with the Release archive, GCC 15, O3/NDEBUG/C++17,
ffp-contract=fast, DAL_USE_EIGEN and EIGEN_DONT_PARALLELIZE. It ran synchronously
on CPU 0 of an i9-13900HX after local builds completed. Ten-round medians:
fast preparation 86.583 us; compatible fast Price 42.916 us; complete one/two-row
fitting 2.809/5.642 ms. For 65536 fit paths, tree/compiled double evaluation
6.533/6.126 ms and tree/compiled AAD 24.015/23.745 ms. MC preparation medians are
148–153 us. Maximum RSS is 12216 KiB. These are new informational baselines,
not paired regression gates or a throughput guarantee. Full product/performance
matrices and Windows/MSVC remain unrun locally; current-head CI and coordinator
review/merge gates remain required.

## Shared documentation drafts: integration pending

DAL-402 owns shared guide/CHANGELOG/artifact-index writes during parallel work.
The following is a concrete draft for serial integration by the coordinator;
none of these shared-file updates is claimed completed. Merge must wait for it.

### Methodology guide addition

Add a Gaussian BondOption subsection to the existing script calibration guide:

The core `dal/model/gsrbondoption.hpp` API imports BondOption once with explicit
instrument ID, currency, discount route and notional. NewBondOptionScriptInstrument
stores concrete expiry, observed bond maturity, strike, direction and N in the
immutable source/header. Subsequent binding, save/load, fast pricing and MC/AAD
all use that source; no native product is retained. It admits the existing
single-payment hard Gaussian template only, with native one/multi-factor GSR
numeraire, same curve snapshot and AtEvent settlement. Extra fees, delayed
payment, fixing-dependent strike, SLV and positive smoothing are refused.

PrepareCalibrationInstrument derives the complete descriptor; PriceGaussianBondOption
validates that plan against the instrument/context and calls the native Gaussian
Black kernel using an ephemeral descriptor-derived BondOption. Source N is
applied once. CalibrateGaussianBondOptions accepts MultiFactor GSR, immutable
Price observations and selected g coordinates, retaining the existing bounded
Gauss-Newton parameter scaling, bounds, prior and smoothing meanings. It always
requires the fast template; it does not run MC in the optimization loop.

Its result exposes the fitted model, raw/scaled data residuals, stable quote axes,
parameter labels, final plan identities and actual model/preparation/price-call
counts. Its owning report carries the existing common axes/errors; numeric
metrics are on the domain result, not new shared report-table APIs.
quoteJacobian means raw dPrice/dg at fixed contract bindings, excludes
penalty rows and is not calibrated quote risk. oneSidedColumns identifies bound
stencils. Zero selected g or a degenerate payoff sets nativeJacobianAvailable
false and leaves the raw Jacobian empty, while double Price can remain available.
After StepStalled, the adapter checks the original projected-gradient criterion
at the final accepted point; it does not change legacy solver behavior.

For independent repricing, copy the resolved preparation context, set its model
to Handle_<ModelData_>(result.model_), and call PrepareCalibrationMc on the
original instrument with explicit sampling/AAD settings. ValidateCalibrationMc
compares independent validation mean/SE and native gradients with a fast
reference, including FD step-refinement indicators. Optimizer convergence,
per-row fit tolerance, fast numerical indicator and MC statistical validation
are distinct decisions. No public/Python/Excel importer, general-script fitting,
caplet/swaption, SLV or quote-risk capability follows from this prototype.

### CHANGELOG draft

- Add core Gaussian BondOption script import, descriptor-derived fast Price and
  selected-g calibration, with same-contract MC/AAD validation and explicit
  degenerate-derivative and model/settlement/smoothing admission boundaries.

### Artifact index draft

Add `implementation/dal-403-bondoption-prototype.md` as the active Gaussian
BondOption prototype implementation/validation record. No new user guide,
renamed document or docs/CLAUDE index entry is needed for this guide extension.
