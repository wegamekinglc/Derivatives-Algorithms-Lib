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
or facade changes are planned. Shared guide/CHANGELOG/index writes were initially
reserved for DAL-402; their subsequent serial integration is recorded below.

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

## Shared documentation integration

After the coordinator released DAL-402's write scope, the Gaussian BondOption
draft was integrated into the existing
[contract guide](../../../docs/methodology/script_calibration_contract.md#gaussian-bondoption-import-and-selected-g-fitting),
the 2026-10-21 CHANGELOG and the active artifact index. The guide includes the
two repaired numerical boundaries and the actual core API limits.

DAL-402's accepted Deposit guide, CHANGELOG entry and index text are retained
from `afb17746c816f31a345e1ce7e35f428e6f4b597b`. Its implementation record is
copied byte-for-byte only to keep the index target available before that PR
lands. No Deposit product source, tests or unmerged product commits are imported.
The existing docs/README, methodology/README and CLAUDE links already point to
this unchanged guide path; no additional guide or index edit is needed there.

This documentation-only follow-up starts at repaired code head
`64f4acfd6fabf42dcf0da81001ad7aa87e727512`. Static checks are
`python3 .github/scripts/check_docs.py`, `git diff --check` and assertions that
the prior Deposit text is unchanged, its artifact copy is exact and every
changed file is Markdown. No product test/build or performance probe is run.
The earlier code/statistics/performance evidence remains applicable; focused
code/document review and current-head CI remain with the coordinator.

## Focused P2 raw-partial and Price-boundary repair

Input head: `37bdb17599d1f11bc03e893bf68c47e23eccea5f`, existing PR #611
and branch `agent/dal-implementer/4a63910df848`. The two review findings are
confined to the new model-owned adapters. DAL-398 section 9 controls owning
diagnostics. The existing Gaussian kernel, optimizer, shared interfaces,
source/binding identities and frozen numerical/statistical settings are unchanged.
That code-repair handoff preceded the shared-document integration recorded above;
documentation acceptance remains a separate merge requirement.

### Design and behavior

Raw Jacobian restoration no longer forms ErrorScale/parameterScale first.
A private adapter helper splits the normalized partial and both finite positive
scales with frexp, combines their bounded mantissas and restores the combined
binary exponent with ldexp. This avoids intermediate ratio/product overflow or
underflow when the resulting native partial is representable. Every published
row/column is checked. Nonfinite input/output or loss of a nonzero partial to
zero raises CalibrationNumericalFailure, rather than publishing an available
nonfinite Jacobian. The Solve diagnostic identifies the exact /quote_jacobian
row/column, native parameter label, instrument, measure, observation and quote;
the existing boundary retains owning report/axes. FD steps, bounds, optimizer
metrics/rank conventions, zero-g unavailable semantics and numerical scales are
not changed. This is unit restoration of the existing FD result, not a new
analytic derivative or stronger floating-point accuracy guarantee.

The independent PriceGaussianBondOption entry catches native Exception_ only
at its new Gaussian evaluation boundary. It emits an owning Evaluate-stage
CalibrationNumericalFailure at /price, retaining original what() text in actual
and message plus instrument/measure IDs. Existing CalibrationError diagnostics
are rethrown intact. Admission and compatibility checks remain in place; native
PriceGSREuropeanOption calls keep their original exception behavior.

### RED/GREEN and directly affected verification

Four new regressions were written before their repairs and failed as expected:

- The review's g=.02, exact fast target, default settings and errorScale=1e308
  returned an infinite raw Jacobian. The repaired value is 33.132851047086234,
  versus 33.132851047130465 at scale 1e-6 (difference below 1e-7). The large
  scale is an input-boundary test only, not a change to frozen acceptance.
- Two selected g knots and ordinary/large-scale observations sharing one quote
  reproduce the defect beyond the first row/column. All four repaired native
  partials agree with the ordinary-scale reference within 1e-7.
- A finite-Price long-maturity/N=1e308 case whose native derivative is genuinely
  unrepresentable previously published infinity. It now throws the owning
  Solve failure with /quote_jacobian/0/0 and complete observation/quote axes.
- Finite g=1e200 passed preparation and leaked InvalidCovariance from the native
  kernel. The new standalone Price error has owning Evaluate report and exact
  cause after input unwind. Both native one-factor and multi-factor entry
  points are checked to retain their original bare exception on the same input.

The three Jacobian tests went from 0/3 to 3/3; the Price test went from a leaked
exception to passing both model cases. Focused executables compile committed
test sources and link the freshly rebuilt Release core/gtest archives:

```sh
cmake --build build/Release-linux --target dal_cpp -j 12
g++-15 -O3 -DNDEBUG -std=c++17 -ffp-contract=fast -I dal-cpp -I dal-cpp/externals/rapidjson/include -isystem dal-cpp/externals/googletest/googletest/include -c dal-cpp/tests/model/test_gsr_bondoption_script.cpp -o build/Release-linux/dal403_repair_test.o
g++-15 -O3 -DNDEBUG -std=c++17 -ffp-contract=fast -I dal-cpp -isystem dal-cpp/externals/googletest/googletest/include -c dal-cpp/tests/model/test_gsr_european.cpp -o build/Release-linux/dal403_repair_legacy_test.o
g++-15 -O3 -DNDEBUG -std=c++17 -ffp-contract=fast -I dal-cpp -isystem dal-cpp/externals/googletest/googletest/include -c dal-cpp/tests/test_main.cpp -o build/Release-linux/dal403_repair_main.o
g++-15 -O3 -DNDEBUG -ffp-contract=fast build/Release-linux/dal403_repair_main.o build/Release-linux/dal403_repair_test.o build/Release-linux/dal403_repair_legacy_test.o build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o build/Release-linux/dal403_repair_tests
```

Final selection passes 15/15: the four new regressions, ordinary descriptor Price
and calibration, owned solver overflow, plan invalidation, one-factor pricing,
bounds/prior/smoothing/selected-knot behavior, zero-boundary unavailable Jacobian,
and three existing native Gaussian Price/invalid-input/degeneracy tests. The full
filter is retained in the attached final log/XML and repair manifest. Both
production TUs pass GCC 14.3 syntax-only Werror with the existing CI warning
categories/exclusions. Modified-range formatting, CCN <=8, diff and changed-record
documentation checks pass. No frozen test or acceptance threshold is weakened.

The original 135-core/16-public and frozen MC evidence in attachment
`01a15e33-32e1-7345-bd84-c27ea56e5e82` is reused within its source scope, not
reported as a new execution. Its record and four source digests match the input
Git blobs. This repair changes only the two adapter TUs, their dedicated tests
and this record; no shared kernel/MC/statistics, header, dependency or build
configuration changes. No complete product/statistics matrix or local Windows
run is repeated. At the code-repair handoff the PR remained draft pending serial
documentation, two-blocker review and exact-head CI.

### Scoped performance and normal-result provenance

Only compatible fast Price and one/two-row fit callers are affected. The original
probe retains CPU 0, one warmup, ten measured rounds, 100 Price calls and ten
fits per round; unchanged preparation and all MC timing cases are excluded.
Input-source objects from the exact reviewed head override the two adapters in
the baseline executable; repaired and baseline probes link the same current
archive and probe object. This is not an input-head archive identity claim.

All warmup/measured one/two-row outputs match exactly: fitted g
0.020000000000000108, raw derivative 33.132851048462733, Price
0.38203940125753538, 20/40 fast calls and checksum 424.64334138329616.
Before/after median microseconds are fast Price 42.90/42.79, one-row fit
2812.41/2792.90 and two-row fit 5480.41/5517.43. Raw samples and binary/source
digests accompany the handoff. These are scoped informational timings, not
calibrated performance-gate passes or speedup claims. Unchanged numerical
kernels, MC and unrelated benchmark matrices are excluded. The implementer
does not merge, dispatch experts or activate downstream work.

## Master integration after DAL-402 merge

The coordinator requested only integration of the existing PR #611 and branch
`agent/dal-implementer/4a63910df848` after DAL-402 PR #610 merged. The clean
original checkout was at 37bdb175; it was fast-forwarded to the exact reviewed
head `a552cdc498db35254bb01e47f5468af83e037a53` before merging current master
`920c8a99a6104ca416d5a88ff4d2faa01c624ebf`. This master contains the accepted
DAL-402 implementation. No rebase, force push or new PR is used.

The merge produced exactly three shared Markdown conflicts: the artifact index,
CHANGELOG and script calibration guide. Keeping the already reviewed BondOption
blocks resolves them: each resolved file is byte-for-byte identical to a552.
The Deposit text in that head already equals master; removing the one
BondOption block from each resolved document reproduces the complete master
document exactly. Both artifact links and both methodology sections remain
once, with their existing valid anchors. The DAL-402 activity record is also
byte-for-byte identical to master. No reviewed prose is rewritten.

Identity verification checks all 2845 tracked tree entries, including modes
and object IDs. Relative to a552, the integration adds only master's three
Deposit product/test files plus this appended activity section. Relative to
master, PR #611 contains only its four reviewed BondOption product/test files,
three shared documents with the reviewed BondOption additions, and this
BondOption activity record. All other entries equal master, including shared
APIs, build/workflow files and submodule pins.

The preserved BondOption Git blob IDs, identical to a552, are:

- `dal-cpp/dal/model/gsrbondoption.cpp`:
  `fc54b451866dea71b660506b6840d554c4fc742b`.
- `dal-cpp/dal/model/gsrbondoption.hpp`:
  `b2804c3ae96710e6057c8a8a55185ac28d1b2db6`.
- `dal-cpp/dal/model/gsrbondoptioncalibration.cpp`:
  `0bb5379384fd8c5e97a0b8b8e156a98022866de3`.
- `dal-cpp/tests/model/test_gsr_bondoption_script.cpp`:
  `4b167f09d1f8011a3f8e5bb06c3e283eb318561b`.

The imported Deposit Git blobs, identical to master, are:

- `dal-cpp/dal/curve/depositscript.cpp`:
  `39d4a94215131c8172a2424506ef4f6c39918cbc`.
- `dal-cpp/dal/curve/depositscript.hpp`:
  `e197de120f7d8e568e4d11997dff8b1f0608b547`.
- `dal-cpp/tests/curve/test_deposit_script.cpp`:
  `94207aea20059276516a8a74af50677e124779c1`.

Static verification commands:

```bash
git fetch origin master agent/dal-implementer/4a63910df848
git merge --ff-only origin/agent/dal-implementer/4a63910df848
git merge --no-ff --no-commit origin/master
python3 build/dal403-master-integration/verify.py
python3 .github/scripts/check_docs.py
git diff --cached --check
git ls-files -u
```

The initial merge conflict is recorded; after resolution the identity checks,
83-document static check, index targets, whitespace check and zero-unmerged
entry check pass. The verification script/JSON and conflict scope accompany
the handoff. This is pure integration: no new behavioral test or production
RED/GREEN change is appropriate. No product/statistics/performance matrix,
new build or local Windows execution is run or claimed.

The coordinator's existing code approval `01a15e55-2feb-726c-b3ec-fe3e6a22d224`
and document acceptance `01a15e5d-d806-79ab-8a35-68d783fcf0a3` remain inputs,
supported by the exact preserved blobs/text. Earlier runtime/numerical evidence
is reused within its recorded source scope; the combined tree is not claimed
to have the previous executable hashes. Current-head CI and final integration
acceptance remain with the coordinator. DAL-402 remains in_review; no PR is
merged and no downstream task is activated by this integration.
