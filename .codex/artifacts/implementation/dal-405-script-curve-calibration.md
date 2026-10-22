# DAL-405: native-coordinate script curve calibration

## Focused review follow-up: methodology

At reviewed head `8b244a0d9ac9e27e59f33f77f3424559f35aeb6a`, the documentation
review requested two small additions. The main script calibration guide now
publishes the new Fit driver's dense Cholesky/fresh-Jacobian choices and its
separation from legacy CG residual-floor/secant-refresh rules. The older
underdetermined guide explicitly scopes its gamma/CG/Broyden discussion to
Find/Approximate and links back to the new entry. Both links are reciprocal.
CHANGELOG and indexes need no additional entry for this correction.

This first follow-up commit changes only these two Markdown guides and this
record. Product directory tree IDs are compared with the reviewed head in the
handoff evidence; no product or performance test is run for this prose change.
The coordinator subsequently added three P2 code repairs to the same serial
batch; their separate change and targeted verification are recorded below.

## Control inputs and source applicability

Startup HEAD and fetched master were
`1f6eaa32424d0c71efb148cfb953f1634ff209cf`, the accepted DAL-404 PR #612
merge. The checkout was clean, on `agent/dal-implementer/4977c0c0fd0c`.
The research baseline `0f1feabee193d0976ab7a5336c9e9e29cb2aa28d` is a
design reference, not the implementation base.

Controlling inputs are DAL-396 research attachment
`01a15c20-3723-7ddd-8ef1-c0ac706cdc16`, sections 4.2–5 and 9–10; the expert
critiques; the DAL-398 API contract; the current methodology guide; and the
DAL-399–404 active records. DAL-404 is merged in both GitHub and Multica.
The parent's continuous authorization and the DAL-405 issue's exclusive
curve/provider/solver scope control delivery. DAL-411 model/matcher files,
the shared definition/compiler/MC protocol and downstream tasks are untouched.

The issue/PR handoff pins the delivery head. The attached source manifest,
build commands and binary SHA-256 values identify the exact tested source;
no historical executable is presented as newly tested.

## Implementation and bounded capability

- `dal-cpp/dal/curve/curveinstrumentcalibration.hpp` defines separate geometry,
  explicit market routes, frozen valuation, ordered observations and solver
  settings. It exposes prepared evaluation, single/ordered-stage solves,
  owned result snapshots and the legacy homogeneous-objective import.
- `dal-cpp/dal/curve/curvescriptprovider.cpp` builds the existing native curve
  representation over candidate component coordinates and a passive base.
  Discount collateral, projection tenor, role and currency are explicit.
  Missing/mismatched routes fail; there is no global market read.
- `dal-cpp/dal/curve/curveinstrumentcalibration.cpp` prepares shared Deposit
  cashflows once per observation, then refreshes candidate values and raw
  native partials. The required GSRCurveData snapshot is only an ingress
  adapter for the frozen P0 preparation interface. It does not price or
  parameterize the candidate curve. Native curve geometry/interpolation and
  base/route data enter the new plan identity independently of that snapshot.
- `dal-cpp/dal/math/optimization/underdetermined.hpp` and its implementation
  add an independent Fit driver. Existing Find/Approximate bodies and all
  their callers are unchanged. The new driver counts FD calls, keeps the
  last validated candidate and has explicit fit acceptance and termination.
- The new curve test file and additions to the existing optimization test
  file contain independent cashflow/local-problem oracles. The methodology,
  CHANGELOG and active-artifact index describe the delivered core surface.

The immutable definition/binding remains the product authority. No alternate
native product is retained by the new calibration path. Prepared cashflows,
capabilities, fixed geometry, fixing policy/snapshot, native routes and numerical
settings are frozen. Repeated candidate evaluations do not reparse source,
rebuild a schedule or reread global history. Candidate native curve objects
are rebuilt to refresh their numeric state.

PWC right-forward and PWLF interleaved left/right coordinates keep their
existing order. LOG_DISCOUNT requires the existing explicit today anchor and
excludes it from theta. ZERO_RATE accepts only future input knots and excludes
its implicit anchor. INPUT/INSTRUMENTS/AUGMENTED geometry is resolved separately
from observation order; pillars cover admitted cashflow dependencies.

FD is the default; Auto selects FD. Explicit AAD uses the same native curve
factory and cashflow expression. All native representations and all log-DF
schemes have FD/AAD checks. Projection/base/background provider derivatives
are checked separately. Unsupported derivative/MC requests fail rather than
changing backend. Raw quoteJacobian is observation-by-native-parameter
dModelMeasure/dTheta; quote-risk remains unavailable.

The new ordered-stage runner enforces one currency/valuation and inherits prior
discount/projection curves by key, preserves explicit stage overrides, and
supplies a projection discount base.
The enabled Deposit template has no forecast FIX, so it cannot constrain a
projection curve. The stage-inheritance test deliberately uses an initial-fit
projection stage; it is not evidence of projection-product calibration.
Generic FIX/product-family lowering remains DAL-406. Live FX/XCCY admission
remains downstream; the mixed-FX-unit test below is an optimizer oracle only.

Deposit valuation is origination with concrete payments. Explicit today policy
and history enter identity; Model and RequireHistorical give the same fixed
cashflows when there is no fixing request. Changed valuation, missing snapshot,
unsupported historical/contingent requests or collateral conflicts fail.
No new general historical FIX or XCCY timestamp policy is claimed.

## Local mathematics, acceptance and compatibility

Theta0 is fixed. Positive p defines S=diag(p), z=S^-1(theta-theta0), and
W_z=S'W_theta*S. Reports retain nativeWeights, transformed weights, theta0,
p, z, native layout, lambda, eta, raw/scaled norms and axes. Underflowed or
overflowed metric entries fail instead of silently changing the metric.

EXACT takes the existing weighted equality-QP direction and accepts finite
outputs with scaled infinity norm <=1, including a candidate whose L2 rises.
APPROXIMATE solves the local normal equation

```text
(W_z + lambda*J_z'J_z) delta = -W_z*z - lambda*J_z'r_scaled.
```

It accepts only scaled L2<=eta, including at the initial point. It does not
certify stationarity, a local/global objective minimum or a unique solution.
Lambda/eta are explicit; row scales never generate a new weight. Solver FD
bumps z; independent prepared evaluation FD bumps native theta.

Numerical implementation choice: the new regularized local solve uses dense
Cholesky and refreshes the Jacobian each iteration. It does not inherit the
legacy CG absolute residual floor or its secant-refresh schedule. These are
new-driver numerical choices, not a claim of matching legacy trajectories.
The matrix oracles validate their local equations, while the old paths still
execute the original algorithms. No large/joint-system scalability claim is made.

Residual attempts, including FD bumps, consume the solver evaluation budget;
gradient attempts have their own count/budget. A final value refresh preserves
the actual model value even when subtraction against a large quote loses
digits. This refresh and an optional at-solution Jacobian have separate
diagnostic counters, outside fit budgets. Initial fit without derivative
diagnostics reports derivative None. Nonfinite measures, coefficients, norms,
raw partials or candidate coordinates cannot produce a successful result.

CalibrationError owns axes and a CurveInstrumentCalibrationCandidate whose
Result retains the last valid state and its resolved spec. Numerical trial
failure preserves the previous validated state; first-evaluation failure does
not invent a candidate. Invalid coordinates carry their component/date label.
Session persistence and calibrated quote risk are explicitly unavailable.

Legacy import stores the original native spec/options, row count, tau and f;
the algebraic mapping is lambda=M*(tau/f)^2, eta=f, S=I. Lambda is an empty
optional when it cannot be represented. Legacy execution remains available
and invokes CalibrateYieldCurve directly, preserving ordering, defaults,
original EXACT boundary and APPROXIMATE unfitted-last-point returns. The new
entry still throws if its own fit gate fails. Tests distinguish those behaviors.

## Independent oracles and red/green evidence

The focused tests keep fixed coordinates, W, scales, weights and budgets:

- EXACT scalar constraint x0+x1=3 under W=diag(1,4) gives (2.4,.6), with one
  counted central-FD local step and six evaluations.
- A two-row regularized linear problem has W=[[2,.5],[.5,3]], theta0=(1,-1),
  scales=(2,.5), lambda=4. Its independent normal matrix is
  [[22,-13.5],[-13.5,20]], RHS=(-.5,20.5), determinant 257.75.
- Rate/Price/FX numeric row expressions normalize to rows (1,1), (1,-2),
  (2,1). W=diag(2,3), theta0=(.2,-.3), lambda=6 produce normal matrix
  [[38,6],[6,39]] and RHS=(60.4,53.1). A Price row re-expression by 1e6 leaves
  that problem unchanged. This does not admit FX script instruments.
- Two .8 scaled residuals have L2=sqrt(2)*.8>eta=1.1 despite each row/RMS
  being below eta. A one-evaluation budget must fail. Exact boundary 1 and
  initial approximate fit require no gradient.
- Deposit cashflow oracles use N=100, K=.025, alpha=365/360, and explicit
  DF/exponential/quotient formulas. A two-node PWLF integral independently
  checks each interleaved Price/Rate column. Nonlinear mixed Price/Rate
  re-expression uses lambda=1e6 and eta=.01, with an independent scalar normal
  iteration and correctly transformed S/W.

Initial compile-only REDs show the missing Fit API and curve header. Runtime
RED logs show owned Evaluate/Prepare boundaries, feasible EXACT candidate
acceptance, legacy unrepresentable mapping, collateral routing, derivative
None at initial fit, metric underflow, model-value cancellation/raw-norm
overflow, parameter-label diagnostics and mixed-currency/valuation stage
rejection. Each has a subsequent GREEN run;
no fit threshold or budget was relaxed.

Test development corrected an invalid exact-root expectation for approximate
early acceptance by replacing it with a tighter independent local-iteration
oracle at the same eta. Native weekend adjustment and natural-cubic minimum
node requirements were fixed in fixtures, not bypassed in production. These
setup corrections are not reported as product REDs.

The final source passes 99 targeted tests: 23 ScriptCurveCalibration, 5 new Fit,
16 legacy Underdetermined, 17 Calibration, 6 CalibrationPlanner,
12 CurveParameterization and 20 existing DepositScript tests. This is the
DAL-405 scoped set, not DAL-404's different P0 acceptance matrix.

Key reproduction commands from the repository root:

```sh
git submodule update --init --recursive
cmake --preset Release-linux -S . -B build/Release-linux -DCMAKE_CXX_COMPILER=g++-14 -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/Release-linux --target dal_cpp gtest -j 10
g++-14 -O3 -DNDEBUG -std=c++17 -fPIE -ffp-contract=fast -I dal-cpp -isystem dal-cpp/externals/googletest/googletest/include dal-cpp/tests/test_main.cpp dal-cpp/tests/curve/test_script_curve_calibration.cpp dal-cpp/tests/math/optimization/test_underdetermined.cpp dal-cpp/tests/curve/test_calibration.cpp dal-cpp/tests/curve/test_calibration_planner.cpp dal-cpp/tests/curve/test_curveparameterization.cpp dal-cpp/tests/curve/test_deposit_script.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o build/Release-linux/dal405_tests
build/Release-linux/dal405_tests --gtest_filter='*-DepositScriptNumericalTest.*'
lizard -l cpp -C 8 -w dal-cpp/dal/curve/curveinstrumentcalibration.cpp dal-cpp/dal/curve/curvescriptprovider.cpp dal-cpp/dal/math/optimization/underdetermined.cpp dal-cpp/tests/curve/test_script_curve_calibration.cpp
python3 .github/scripts/check_docs.py
git diff --check
```

All three production TUs also pass the existing Linux CI
Wall/Wextra/Wpedantic/Werror policy with its existing interface/generated-code
exclusions. No warning policy was changed. Modified code passes formatting
and CCN<=8 checks. Complete commands, runtime RED/GREEN logs and final XML
are in the evidence attachment.

## Build and performance evidence

Configuration: GCC 14.3.0, CMake 4.2.3, C++17 Release/O3/NDEBUG,
ffp-contract=fast, static native AAD, Eigen ON, native architecture OFF.
Public, examples, benchmarks, Python, Excel and portable Excel are disabled;
AAD diagnostic/profiling/sanitizer modes are off. Host: i9-13900HX, x86_64.
Submodule revisions match the pinned startup revisions: Eigen 3147391d,
gtest d72f9c8a, Machinist e76b0ef2, pybind11 8a099e44, rapidjson ea152ded.
Full revisions, CMake cache, compile database and executable/source hashes
accompany the evidence.

Final rebuilt SHA-256 identities:

- Core archive: `5aa2eb94e32ffd093ba408f9048c68e94b777530318b84287f174fa65f855764`.
- Targeted tests: `9068df979b985b45f76327d0c62837f806e176259bf593827fb98700d4d940e9`.
- New-provider probe: `8ff2b3da9463384b3e6df0c0bd4999eaa7e2484aa9e20fb57e068f73d2c92dc3`.
- Legacy head probe: `4c6ccd219b5088a84ade084c2efff6e4e5458bcd0412c5e0f83b1e739a2b9818`.
- Legacy baseline probe: `b63f894dcdce254dbea7ac92a2d78044268b97689d84f8f12127d3ece2fa675b`.

Changed-provider callers select prepare, double value, FD/AAD raw Jacobian
and FD/AAD solve at 1 and 16 Deposit observations/native parameters. Each case
uses three warmups and ten retained samples; the production default 200/20
budgets are preserved. The attached probe records all samples and checksums,
including 1-observation solve counts FD 10/3 and AAD 4/3, and 16-observation
counts FD 133/4 and AAD 5/4 (evaluations/gradients). Timings establish an
informational baseline, not a regression gate or a general AAD speed claim.

Legacy compatibility callers select existing 23-swap LOG_LINEAR EXACT and
APPROXIMATE cases, preserving their one warmup/five samples. Ten paired process
runs alternate baseline/head order. The isolated legacy baseline recompiles
only the startup underdetermined.cpp with identical compile flags and replaces
that archive member; all reachable other sources/dependencies are unchanged.
New curve objects are unreferenced by the legacy probe. Every paired numerical
checksum matches exactly. Median reported EXACT time is 1.227/1.2305 ms and
APPROXIMATE time is 7.8055/7.798 ms (baseline/head), informational only.
This target is outside the repository's closed benchmark regression gate.

No full benchmark matrix, joint/XCCY/Gaussian/MC timing, full statistical
P0 matrix, public/MSVC build or standalone/example topology run is claimed.
There are no example, build-list, binding, model or shared compiler edits.
Existing P0 numerical/model conclusions remain scoped to DAL-404's accepted
source/configuration evidence; they are not rerun verdicts for these binaries.
Coordinator review and exact-head CI remain delivery gates; the author does
not merge or start downstream work.
