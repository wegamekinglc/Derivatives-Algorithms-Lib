# DAL-404 P0 same-source independent acceptance

## Verdict and controlling scope

**PASS for both bounded P0 chains. No remaining product blocker was found.**
Deposit Price/ParRate and Gaussian BondOption Price pass the independent
oracle–fast, oracle–MC and fast–MC value/native-derivative comparisons below.
Each chain starts with one immutable definition/binding, calibrates through
its descriptor-derived fast adapter, and reprices the original instrument
through script MC/AAD. Acceptance remains subject to coordinator review of
this test/validation PR and its exact-head CI; this record does not authorize
starting DAL-405/411 before that PR is merged.

This is P0 acceptance, not general script calibration, general curve/XCCY
providers, SLV equivalence, public/binding delivery or calibrated quote risk.
No product, shared API, build, workflow, published guide or CHANGELOG changed.
This active artifact controls the DAL-396 dependency frontier.

Control inputs: DAL-398 API note, DAL-399–403 implementation artifacts,
`docs/methodology/script_calibration_contract.md`, and research attachment
`01a15c20-3723-7ddd-8ef1-c0ac706cdc16`, sections 7.1, 7.3, 8.1–8.3 and 10.
Read the current DAL-396 authorization and scheduling thread
`01a15c2f-2f07-7edb-82b9-10fcb9b29331` and DAL-403 final merge/start comment
`01a15ed0-3ca3-7430-8e46-a78bc19a8751`. Prior expert decisions are inputs;
their completed primary reviews were not reopened.

## Revision and evidence applicability

Clean starting master/product HEAD:
`8e5792f08d6522d5047b105f3806732f1e715f72`, on the dedicated branch
`agent/dal-tester/231b9a76edf7`. It contains DAL-402 merge
`920c8a99a6104ca416d5a88ff4d2faa01c624ebf` and DAL-403 merge
`8e5792f08d6522d5047b105f3806732f1e715f72`. Research baseline
`0f1feabee193d0976ab7a5336c9e9e29cb2aa28d` is historical only.

Reused evidence was fetched through `multica attachment download`, then
checked against Git and the raw logs/XML rather than accepted from summaries:

| Evidence            | Attachment                             | Checked revision and result                                                                                  |
|---------------------|----------------------------------------|--------------------------------------------------------------------------------------------------------------|
| Deposit original    | `01a15e2c-157a-7724-9a5a-faf10ae052fb` | `9243a880fa5c4f4db53e731cc813209285f25f71`: 139 core, 15 public; 14 frozen Price/ParRate rows                |
| Deposit repair      | `01a15e42-b39d-784a-9290-4e694a18ad50` | `afb17746c816f31a345e1ce7e35f428e6f4b597b`: 24 focused checks, five behavioral RED/GREEN cases               |
| BondOption original | `01a15e33-32e1-7345-bd84-c27ea56e5e82` | `37bdb17599d1f11bc03e893bf68c47e23eccea5f`: 135 core, 16 public; six frozen CALL/PUT rows and end-to-end fit |
| BondOption repair   | `01a15e50-44ee-7823-87d6-f8f3616c8c34` | `64f4acfd6fabf42dcf0da81001ad7aa87e727512`: 15 focused checks, four behavioral RED/GREEN cases               |
| Master integration  | `01a15e8c-c55a-7b40-8db9-e3e780d74fa5` | `425d65c98abc28be4903d0de6f6377961e7285e1`: preserved reviewed blobs/docs                                    |

The independent audit verifies all 38/35 evidence digests in the two repair
manifests, their three/four changed Git files, original zip hashes, the
original Deposit source/test patch against Git, original BondOption source
digests, and the integration blob claims against the actual starting master.
There are exactly five added production files relative to merged DAL-401
`15a3b445362062e75099de3bf9b4353fa2225873`; all other core production files,
shared script/math/generated runtime, dependency pins and root build inputs
are unchanged. The five additions are the two Deposit files and three
BondOption files. Final source blobs include:

- `dal-cpp/dal/curve/depositscript.cpp`:
  `39d4a94215131c8172a2424506ef4f6c39918cbc`.
- `dal-cpp/dal/curve/depositscript.hpp`:
  `e197de120f7d8e568e4d11997dff8b1f0608b547`.
- `dal-cpp/tests/curve/test_deposit_script.cpp`:
  `94207aea20059276516a8a74af50677e124779c1`.
- `dal-cpp/dal/model/gsrbondoption.cpp`:
  `fc54b451866dea71b660506b6840d554c4fc742b`.
- `dal-cpp/dal/model/gsrbondoption.hpp`:
  `b2804c3ae96710e6057c8a8a55185ac28d1b2db6`.
- `dal-cpp/dal/model/gsrbondoptioncalibration.cpp`:
  `0bb5379384fd8c5e97a0b8b8e156a98022866de3`.
- Original merged BondOption test:
  `4b167f09d1f8011a3f8e5bb06c3e283eb318561b`; its numerical test is expanded
  in this PR, with no production change.

Historical executable identities remain historical. Original core test
SHA-256 values are Deposit `b9da51143f9328ab6fd4ef7f9fb93e327d95921333c7729e1e7a595a7c4fe540`
and BondOption `746d58b29ed3412aa8be4b990cbaedb8b63c316e14f8acca1282a2e5231df2e0`.
Repair executable identities, baseline override-object provenance and unchanged
ordinary results are retained in their manifests. No old executable is called
the integrated/current executable. Review approvals
`01a15e48-2313-7b45-94d2-73660175abc0` and
`01a15e55-2feb-726c-b3ec-fe3e6a22d224` are reused within that scope.

Repair-manifest cache values are authoritative where earlier prose differs:
benchmarks were enabled in both repaired build configurations, and portable
Excel was enabled in the BondOption repair configuration. Those extra targets
were not part of the focused execution and do not alter core math flags.
Original numerical results retain their recorded executable/configuration
scope; the fresh integrated probe uses the explicit configuration below.

## Frozen data, independent oracles and statistics

Both fixtures use USD OIS, V=2026-10-21, H=1, hard payoff and the native
Gaussian model/numeraire. The oracle reads the original financial terms;
it never calls the exporter, descriptor matcher, Gaussian pricer or
`ValidateCalibrationMc`. Source identity alone is not its correctness proof.

Deposit has S=V, T=2027-10-21, N=100, K=.025, alpha=365/360 and native
theta=log DF(T). Independently use B=-N+N*exp(theta), A=N*alpha*exp(theta),
Price=B+K*A and ParRate=-B/A. Their theta derivatives are elementary
exponentials; expected native-g derivatives are zero. This does not assert
zero pathwise g risk. Initial theta=-.04, non-root=-.06, fitted
theta=-log(1+.035*alpha), and fitted +/-1e-4, +/-5e-5 give seven states.
Analytic/native AAD and central FD(h,h/2) checks use the original 1e-10
value, 1e-9 derivative and 1e-6 FD tolerances.

BondOption has E=2027-10-21, T=2028-10-21, log discounts (0,-.04,-.08),
N=100, K=.97, named factor `level`, and constant g. E-to-T is 366/365.
Independent normal-CDF mathematics uses sigma=g*366/365,
F=DF(T)/DF(E), d1=log(F/K)/sigma+sigma/2, d2=d1-sigma and direction d:
PV=N*d*(DF(T)*Phi(d*d1)-K*DF(E)*Phi(d*d2)); native vega is
N*DF(T)*phi(d1)*366/365. Replacing the random bond by F is not an option
expectation oracle. Fast values use 1e-10 tolerance; FD(h=1e-4,h/2=5e-5)
is compared with independent vega to 2e-4. Its refinement indicator is
included in the MC gradient budget; CRN/pathwise checks use 1e-5/5e-6 bumps.

Frozen common stream: MRG32 seed 1024, `dal.mrg32-normal-paths/1`, Precise
inverse normals, no Brownian bridge, `dal.native-gsr/1` numeraire. At each
advancing timeline sample, normal coordinates are consumed in factor order.
Fit reserves eight batches of 8192 normal paths, [0,65536); validation uses
16 batches of 16384, [65536,327680). Maximum is 327680 per view including
the reserved fit range. Fast calibration executes no MC fit paths. Different
products do not acquire a common economic Brownian path merely from seed
equality. The existing 48-path test compares absolute seeks with sequential
normal generation and an independent path/numeraire formula.

Value sigma=3, nu=nuG=.25, epsilonV=1e-10; Price scale=.1 amount;
Deposit ParRate scale=.001 rate. Deposit native scales are (1,.01),
epsilonG=(1e-7,1e-7); BondOption scale=.01, epsilonG=1e-8. Thus Price
value budget is .025, Rate value budget .00025 and BondOption native-g
precision budget 2.5. No scale, sampling, epsilon or budget was changed.
Check parity, 3*SE precision, total abs(delta)+3*SE value error and native
gradient parity/precision separately. These are statistical indicators,
not rigorous rounding/probability bounds.

MC mean/gradient SE uses the sample SD of batch means divided by sqrt(K),
not path dispersion. Affine ParRate pools coefficient means as -Bbar/Abar;
it does not average batch ratios. The full paired (B,A) covariance includes
the nonzero cross term. Its native derivative is
-dBbar/Abar+Bbar*dAbar/Abar^2, with uncertainty propagated from the full
(B,A,dB,dA) batch covariance. The synthetic independent influence-function
test checks both SEs, and denominator sign/confidence failures are explicit.
The eight-batch prefix is a correlated refinement check, not an independent
second validation stream.

## Three-way numerical findings

**Deposit PASS, reusing validated provenance.** All 14 Price/Rate states
and both native derivative columns pass. This audit independently recomputes
the archived oracle values, all price/theta/g statistical gates and reported
value error indicators. The script-derived fast results, A/B and native
AAD/FD checks remain covered by the exact original/repair source tests.
The 3.5% ParRate fit takes six fast evaluations and returns
theta=-.034870989026579077. Fixed K=.025 retains PV=-.97914291460338632;
only explicit QuoteCopy creates the new K=q binding with zero PV.

At the fitted state, validation Price=-.97713864940166784,
SE=.0019421402168718484, oracle=-.97914291462679159 and total indicator
.007830685875739303<.025. Rate=.034979328421820256,
SE=.000020030428224354413, oracle=.035 and indicator
.00008076286285280332<.00025. Initial full/prefix changes .00288902735
amount and .00003010257 rate also meet frozen checks. Price executes 16
validation engine runs; Rate executes 32 because two coefficient views are
priced. Peak tape nodes are 78/71, retained capacity 1966080 bytes.

**BondOption PASS, with the actual gap filled.** The original six rows
covered initial .012, non-root .016 and .02, but not valid g bumps. They
tested MC against fast FD, with a separate deterministic oracle check.
The audit now also directly checks those six archived MC rows against
mathematical price/vega, conservatively accounting for six-decimal log
rounding. No transitive tolerance argument substitutes for the third edge.

The expanded existing numerical test fits each original CALL/PUT instrument,
then checks initial, non-root, actual fit and fit +/-1e-4, +/-5e-5. All 14
rows pass both direct oracle–MC and fast–MC gates plus deterministic
oracle–fast price/vega checks. CALL fitted g=.020000000000000108;
PUT fitted g=.019999999999999726. Selected fitted rows:

| Side | Independent/fast Price | MC mean       | MC SE         | Independent vega | MC gradient   | Gradient SE   |
|------|------------------------|---------------|---------------|------------------|---------------|---------------|
| CALL | .382039401258          | .381819953179 | .001835220681 | 33.1328510832    | 33.1398496364 | .130885698375 |
| PUT  | 1.26698036037          | 1.26311068159 | .002234968953 | 33.1328510832    | 32.9626420049 | .083756387693 |

Full-precision XML preserves definition/binding/instrument/context/plan/mapping
identity, each native-g value and all 224 batch value/gradient means and
ranges. Independent Python recomputation checks their means, both SEs,
nonoverlapping reserved validation intervals and every frozen gate. Each
side keeps one instrument and mapping while all seven context/plan identities
change. The attached audit JSON contains every row and comparison margin.
The original one-row end-to-end fit uses 20 fast Prices/models/preparations,
zero MC paths; independent validation uses 262144 paths, 16 runs, 87 peak
tape nodes and 1966080 retained tape-capacity bytes.

## Admission, ownership and unavailable edges

The following exact-source tests/results are reused, supplemented by the
focused current-binary checks listed below:

- `DepositScriptTest.TestSourceChangesReachFastAndBothScriptBackendsWithoutNativeEdits`,
  `TestExplicitQuoteCopyRepricesNewMcBindingAndLeavesOldPlanUsable`, and
  `CalibrationMcTest.TestRebindingKAndNChangesAllBackendsAndPreservesOldPlans`:
  source N/K/ALPHA and explicit bindings affect fast/tree/compiled/AAD;
  old bindings/results remain unchanged and reject mismatched instruments.
- `CalibrationPreparationTest.TestBindingSourceHeaderAndContextInvalidatePreparedIdentity`,
  `TestFixingsAndMutableModelContentInvalidateContext`,
  `CalibrationMcTest.TestContextChangesRequirePreparation` and BondOption
  `TestPlansInvalidateBindingCurveModelAndSmoothing`: source, header, curve,
  routes, fixings, evaluation date, convention token, model content and
  smoothing invalidate cached plans. Restored contracts rebuild plans.
- Complete AST tests reject extra fee/payment/receiver, nonlinear or
  fixing-dependent strike, branch-dependent parameters and delayed payment
  from the bounded fast matcher. Same explicit scenarios verify PAYS amounts,
  numeraire division and delayed discount for both tree and compiled engines;
  that script capability does not admit a delayed option to the fast matcher.
- Missing/non-Gaussian or SLV models and incompatible currency/curve/routes
  produce `UnsupportedFastEvaluator` or `CalibrationContextMismatch` at the
  applicable preparation/compatibility boundary. Those MC/fast edges are
  **unavailable**, not failed statistical comparisons or zero risk. XCCY MC
  has no matching P0 model and is unavailable. Zero-g/degenerate options
  retain double Price; requested two-sided/pathwise derivatives are
  `DerivativeUnavailable` or `nativeJacobianAvailable_=false`. Feasible
  solver-bound stencils are explicitly one-sided.
- Tiny precision budgets fail `NumericalValidationFailed` with
  `budgetExhausted=true`; insufficient path budgets fail `CalibrationBudgetExceeded`.
  Unstable annuity confidence fails `InvalidCalibrationAnnuity`. Failure is
  independent of fit convergence and never relaxes scales to pass.
- Deposit partial overflow and Solve failure retain owned diagnostics, axes,
  cause and last fully validated candidate/count; failures before the first
  valid candidate publish none. Gaussian native exceptions and unrepresentable
  raw Jacobian entries retain owned Evaluate/Solve errors and row/column/native
  label. The raw Jacobian is dPV/dg, restored from normalized solver units;
  it excludes penalty rows and is not calibrated quote risk.
- `CalibrationMcTest.TestTapeCleanupAfterPathFailureAndNestedScopeRejection`
  verifies overflow cleanup, subsequent identical gradients and preservation
  of a caller's active recording. Multi-adjoint mode is rejected without
  changing caller mode. No failed-path tape state is reused.

## New execution, test qualification and repair distinction

**Running existing tests:** ten selected current-binary checks pass: seven
BondOption tests for complete descriptor Price, owned native errors, stable
raw Jacobian restoration (including all rows/columns and unrepresentable
partials), model/binding/smoothing invalidation and explicit PAYS scenarios;
three shared MC tests for absolute normal-path seeks, affine covariance and
tape cleanup. Historical 139/15, 135/16, 24 and 15 results above are reused,
not new executions or disjoint test totals.

**Authoring tests:** expand one existing `GSRBondOptionNumericalTest` and add
its full-precision record helper. The expanded test passes on unchanged
production. To qualify the new coverage, a disposable test-only mutation
freezes fast reference context at fitted g only near the fitted state.
Initial/non-root/fit pass; the first new +1e-4 cell fails by
.003315150343922435 against the original 1e-10 price tolerance. The restored
test passes. This is a mutation RED/GREEN, not a product defect or product fix.

**Repairing failures:** no product repair. An initial test draft mistakenly
applied the 2e-4 fine-FD/oracle tolerance to the coarse/fine refinement
indicator itself. At .012 that indicator is .000350679 while fine-FD error
is .000116889<2e-4. The uncontracted assertion was removed; the original
2e-4 oracle tolerance, FD steps, refinement contribution and frozen MC
gradient budget remain intact. The draft failure log is retained separately.
The first CMake configure also ran before submodules finished and failed
on missing gtest; the repeated configure after initialization succeeds.

## Commands, new build identity and exclusions

Run from the repository root; `../evidence/` denotes the downloaded/local
evidence bundle, delivered as an issue attachment, not a runtime path link.
Raw commands/logs, XML, audit program/JSON, mutation and provenance accompany
the handoff. Key commands are:

```sh
git submodule update --init --recursive
cmake --preset Release-linux -S . -B build/Release-linux -DCMAKE_CXX_COMPILER=g++-15 -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/Release-linux --target dal_cpp gtest -j 10
g++-15 -O3 -DNDEBUG -std=c++17 -fPIE -ffp-contract=fast -I dal-cpp -I dal-cpp/externals/rapidjson/include -isystem dal-cpp/externals/googletest/googletest/include dal-cpp/tests/test_main.cpp dal-cpp/tests/model/test_gsr_bondoption_script.cpp dal-cpp/tests/script/test_calibration_mc.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o build/Release-linux/dal404_tests
build/Release-linux/dal404_tests --gtest_filter='GSRBondOptionNumericalTest.TestFrozenOracleFastAndSameBindingMcAtInitialNonrootAndFit' --gtest_output=xml:../evidence/dal404-bond-matrix.xml
python3 ../evidence/dal404_audit.py
lizard -l cpp -C 8 -w dal-cpp/tests/model/test_gsr_bondoption_script.cpp
python3 .github/scripts/check_docs.py
git diff --check
```

The ten-test exact filter is preserved in `dal404-targeted.log` and the
bundle's `commands.sh`. Modified-range clang-format comparison and CCN<=8
checks pass. The focused test sources link a freshly built core/gtest;
unrelated test translation units were not built or run.

New environment: GCC 15.2.0, CMake 4.2.3, C++17 Release/O3/NDEBUG,
ffp-contract=fast, static native AAD, Eigen ON, native architecture OFF,
public configured ON, examples/benchmarks/Python/Excel/portable Excel OFF.
Diagnostic/profiling/sanitizer options are OFF. Dependency commits:
Eigen `3147391d946bb4b6c68edd901f2add6ac1f31f8c`,
gtest `d72f9c8aea6817cdf1ca0ac10887f328de7f3da2`,
Machinist `e76b0ef243ca9bd506bb2f3743d8c9b52ee8d01d`,
pybind11 `8a099e44b3d5f85b20f05828d919d2332a8de841`,
rapidjson `ea152ded01708ff1657b275ed0bcbf0681d64519`.

New SHA-256 identities:

- Core archive: `a36d7c5baf8d31bbe0f6942f4f5725b2ae2ed97876ba73b6a00996ec5d0415ab`.
- Focused executable: `4589fb77ad9a4ddadafe640c092e5232bc98beaca4ddabf2cbef750c13ad8929`.
- Modified test source: `4c18ac5a6035240574009e6d7c61bb4f7ff007360fec38c4fa479047ba5ba5f5`.

Performance selection: changed callers are only test assertions/recording;
production algorithms and entry points are identical to accepted P0 sources.
No benchmark case or new performance claim is justified. Existing two/three-node
Deposit and one/two-observation Gaussian information baselines retain their
original executable/configuration scope; they are not regression-gate verdicts.
No full product/statistical/performance matrix, local MSVC, public rebuild or
example/standalone/public-disabled topology build was repeated. There are no
example/build edits. Coordinator review should focus on the new numerical cells,
oracle independence, evidence applicability and this P0 verdict.
