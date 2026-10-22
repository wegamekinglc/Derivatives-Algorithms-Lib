# DAL-412: selected-g script MC calibration

## Delivery state and authority

**Partial implementation; general Price admission and the arithmetic-average
candidate remain blocked on the shared-source handoff. Do not merge this as
complete DAL-412 or start DAL-413.** The final issue comment supplies the unique
draft PR and exact head. No reviewer was dispatched by the author.

Clean input HEAD and fetched master were
`f2dfa8e9cb4eb8508bd3425676e90c2cb513ee86` (merged DAL-411 / PR #616), containing
DAL-401 merge `15a3b445362062e75099de3bf9b4353fa2225873` and DAL-405. The research
baseline `0f1feabee193d0976ab7a5336c9e9e29cb2aa28d` is historical only.
Branch: `agent/dal-implementer/e8919a25b089`, base `master`.

Read before implementation: research attachment
`01a15c20-3723-7ddd-8ef1-c0ac706cdc16` §§4–5, 7.4, 8–10 and expert critique;
[DAL-398 API](../api-notes/dal-398-script-contract.md);
[current methodology](../../../docs/methodology/script_calibration_contract.md);
DAL-399–403/405/411 implementation records and DAL-404 acceptance; parent
authorization/scheduling thread and both current issue descriptions; implementer,
code/test/component, publication and performance references; relevant source/tests.
Research was downloaded through `multica attachment download`.

DAL-406 still owns `calibrationpreparation.{hpp,cpp}`, `instrument.cpp`, its two
shared tests and curve/provider paths. This branch changes none of them. Shared
methodology, CHANGELOG and artifact indexes are also untouched as instructed.
The proposed documentation text below awaits their serial window.

## Design and implemented behavior

`dal/model/gsrscriptcalibration.hpp` provides the separate core
`CalibrateGSRScriptPrices` entry. It accepts native-unit Price observations,
selected `(factor,knot)` g coordinates, explicit MC opt-in/budgets, AAD rows or
basket FD, and optional held-out observations. Preparation still goes through
the existing authoritative shared admission; this driver does not admit a new
product by itself. Existing fast/native entry points and vanilla vol conversion
are unchanged. Known vanilla option targets remain nonnegative; general signed
Price targets are not globally prohibited. Calibrated quote risk is unavailable.

The existing bounded Gauss–Newton implementation is reused unchanged. Solver
coordinates are `x=g/parameterScale`, data rows are `(meanFit-target)/errorScale`.
Prior and knot-gap smoothing penalize movement from the immutable initial g,
including selected-to-unselected neighbors. Their analytic rows have a separate
axis and objective subtotal. The returned M×N `quoteJacobian_` is populated
directly in raw Price/g coordinates; scaling affects a separate solver matrix.
Bounds are materialized in native units, including exact bound endpoints;
native FD denominators use the actual feasible native distance. Rank diagnostics
exclude penalty rows. FD columns report their feasible one-sided stencil.

Each Jacobian uses M AAD row evaluations or 2N complete basket bumps. A run per
row consists of the configured number of batch executions. Final fit,
validation and raw-Jacobian diagnostics are also counted, including repeated
work; these counters are not a claim of an optimized basket. Preparations,
admission refreshes, baskets, Jacobians, AAD/double rows, batch runs, paths and
peak tape nodes/capacity are explicit. No union timeline or multi-output
optimization was introduced.

Prepared MC scripts are immutable shared resources. `RefreshG` admits a new
candidate/context, verifies only g values changed, checks the actual timeline
and normal-coordinate mapping, and shares the compiled/tree structure. Curve,
geometry, H, correlations, valuation/fixings/conventions, binding and smoothing
are frozen. Admission is repeated and counted; this is compilation reuse, not
a claim that all preparation/parsing work disappeared. Models, evaluators,
historical state, numeric inputs and scalar recording scopes are fresh per run;
no active values or writable evaluator are shared between candidates/workers.
Execution remains synchronous on the calling thread.

Fit uses fixed MRG32/Precise paths. Validation follows the fit reservation;
optional held-out follows validation, in a third disjoint range. Held-out is
marked separately and cannot be supplied to `ValidateCalibrationMc` as ordinary
validation. It uses `abs(meanHeld-targetHeld)+sigma*SEHeld<=errorScale` and never
updates fit values or derivatives. Missing held-out input sets requested=false,
passed=false (NotRequested), rather than manufacturing success.

`converged_`, fit tolerance, numerical validation and held-out acceptance are
independent. MaxIterations, StepStalled and NoDescent return a candidate and
honest flags. Invalid inputs/execution and exhausted basket budgets throw owning
reports with observation axes. Budget failure is Solve-stage and retains the
last completely evaluated candidate model; this can be a diagnostic bump, not
necessarily the last accepted optimizer iterate. The diagnostic records the
completed basket count. Gradient parity/precision is requested for AAD mode;
FD-mode statistical validation is value-only, with FD row checks separate.

## Frozen verification inputs

Small driver fixture, declared before its tests: USD N=100, BondOption K=.97,
valuation=2026-10-21, expiries=2027/2028-10-21 and maturities one year later.
Log discounts at valuation/annual nodes are 0/-.04/-.08/-.12; one factor `level`,
H=1, g knots at valuation and 2027-10-21. Initial g=(.012,.016), fixed-fit
reference g=(.02,.025); selected columns deliberately reverse knot order.
MRG32 seed=1024, fit=2×64, validation=4×128, held-out=4×128, total=1152 paths/view;
Price scale=.1, parameter scale=.01, sigma=3, nu=nuG=.25,
epsilonV=1e-10, epsilonG=1e-8. CRN FD native h=1e-6 and h/2=5e-7 at a non-root
regularized candidate. No numerical acceptance is inferred from this small fit
budget. Scale=1e-5 is a separate, intentionally failing precision fixture.

The numerical success/failure separation reuses the previously frozen
BondOption protocol: fit=8×8192, validation=16×16384, same seed/scales/floors;
held-out adds 16×16384, for an explicit total reservation of 589824. The
independent held-out oracle uses the mathematical Gaussian bond-call formula,
not the native pricer or lowerer. Bound-constrained fit can fail its quote
tolerance while independent numerical validation passes; an exact fixed-path
fit can fail its frozen numerical budget. No scale/path/SE floor was relaxed.
The old Swaption `.0426379743 > .025` precision rejection remains a rejection.

The blocked arithmetic-average probe freezes N=100, K=.04, two future USD
6M Libor observations at 2027-10-21 and 2028-04-21 with weights .5/.5, and one
payment at 2028-10-21. Valuation=2026-10-21, flat log discount -.12 at
2029-10-21, one factor g=.02/H=1. The planned numerical protocol is the larger
589824-path reservation above, Price scale=.1, parameter scale=.01,
sigma=3, nu=nuG=.25, epsilonV=1e-10, epsilonG=1e-8, CRN FD h=1e-6/5e-7.
These choices precede any admitted candidate values. No candidate value,
gradient, calibration or numerical success has been measured or claimed.

## RED, GREEN and verification

Focused compilation REDs established missing held-out, refresh, driver and
stencil APIs. Behavioral REDs then established:

- Refreshed and rebuilt plans had different identities; both now use the same
  execution-identity constructor, independent of refresh history.
- Held-out could masquerade as validation; the validation boundary rejects it.
- Vanilla negative targets and held-out Scalar measures were accepted; both
  are now rejected at preparation, with held-out preflight before fitting.
- Budget exhaustion was mislabeled Input and lost its candidate; Solve-stage
  owning reports preserve the last completed candidate and axes.
- Normalized bounds produced native .026999999999999996 below lower=.027;
  exact endpoint materialization also covers the upper=.031 rounding case.
- An unresolved native FD step had Input stage and no parameter label; it now
  retains Solve stage, the exact native g label, candidate and observation axes.

Each behavioral RED exited 1, was inspected, and was followed by GREEN at the
same assertion. Tests were not weakened. A compile command's initial wrong
gtest archive path and an unquoted zsh wildcard were harness errors, not RED
evidence. New code was refactored while green; modified ranges are formatted.

Initial PR head `5619de7bce22589714d89d36675113af55be0503` received four Codacy
complexity findings at its stricter limit of 8: settings validation, penalty
assembly, constructor validation and the non-root FD test. The repair separates
validation responsibilities, copies complete matrix rows and expresses test
bumps with a displacement vector. All original checks, assertions, paths,
scales and tolerances remain. Local analysis of the new driver and its tests
now meets CCN<=8; existing touched MC files retain their CCN<=10 local check.
Current-head external CI still requires its own result; the initial failure is
not represented as an external pass.

Commands (full compile/link commands, raw logs and XML are in the evidence bundle):

```sh
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux --target dal_cpp -j 8
build/Release-linux/dal412-scoped-tests --gtest_filter='GSRScriptCalibrationTest.*'
build/Release-linux/dal412-scoped-tests --gtest_filter='CalibrationMcTest.*:CalibrationMcNumericalTest.*'
build/Release-linux/dal412-scoped-tests --gtest_output=xml:../dal412-scoped-green.xml
git diff --check
lizard dal-cpp/dal/model/gsrscriptcalibration.cpp dal-cpp/dal/script/calibrationmc.cpp dal-cpp/dal/script/calibrationmcstatistics.cpp -C 10
python3 .github/scripts/check_docs.py
```

The scoped executable links the current core archive, repository test main and
the six relevant test translation units: driver, European/native calibration,
BondOption script, rate-option script, MC and preparation. It is a dedicated
binary, not an assertion that the full `dal_cpp_tests` target was built/run.
Final scoped result: **128/128 passed**, including 14 driver tests, 27 MC tests,
29 preparation tests and 58 native/fast/template tests. It covers tape cleanup/nested-recording recovery, exact labels, CRN path oracle,
binding/history, original fast/native/vol behavior, frozen numerical matrices,
and the new driver states. Final counts and immutable executable/source hashes
are recorded with the delivery. Full repository, facade, Windows and binding
matrices were not run; no example/build registration changed.

Build: GCC 15.2.0, CMake 4.2.3, Release, native AADET, Eigen 3.4.0, native-arch
OFF, public enabled, examples disabled; direct test/probe TUs use C++17/O2 while
the core archive uses the preset's O3. All five submodules are exactly at their
repository pins. Changed production TUs also pass GCC 14.3/15.2 syntax checks
with the repository's warning-clean categories/exclusions; this is not a claim
of a full GCC-14 library rebuild. CI and focused review remain coordinator gates.

## Caller performance and memory

Affected callers are the new Price driver and existing MC prepare/evaluate;
native Gaussian pricing/optimizer kernels, curve/XCCY algorithms, tape allocator
and RNG kernels are unchanged. The minimum probe compares one/two observations,
N=2, compiled double/basket FD versus compiled scalar AAD using the small frozen
fit budget. It measures prepare, candidate-refresh/evaluate and full Jacobian
work separately. Both Jacobian methods include their real admission refresh;
M AAD rows are compared with 2N basket bumps, without sharing cross-product
paths. Ten repetitions alternate method order, pinned to CPU 4 on an Intel
i9-13900HX; raw samples and source accompany the bundle.

Final informational medians in microseconds (prepare/evaluate/Jacobian):
one row FD=167.619/284.759/1101.810, AAD=167.935/328.333/316.849;
two rows FD=348.221/570.054/2305.860, AAD=362.077/665.763/642.703.
One-row Jacobians execute FD 8 batches/512 paths versus AAD 2/128; two-row
Jacobians execute FD 16/1024 versus AAD 4/256. AAD tape peaks are 89/96 nodes,
1,966,080 capacity bytes. Process RSS high-water is 650208 KiB, including global
initialization and previous operations; it is not per-call allocated memory.
The final binary/source manifest and raw samples identify this build. No performance threshold or speedup
acceptance is asserted; these probes are outside the nine-target regression gate.
No basket optimization is required for correctness or proposed on this evidence.

## Required serial handoff: exact scope and proposed glue

The official arithmetic-average probe exits 1 with
`UnsupportedCalibrationProduct: /source/rows; expected P0 payments with optional
leading receiver initialization`. The typed immutable definition is accepted;
`Payments` rejects the first running-average assignment. Native fast matchers
must continue rejecting this product. The next shared-source window needs:

1. `calibrationpreparation.hpp`: append an empty, derived-only
   `CalibrationScriptMcDescriptor_` tag to `CalibrationFastDescriptor_`. It holds
   no native product, economic fields, second source or evaluator. Existing
   descriptor alternatives retain their meaning.
2. `calibrationpreparation.cpp::PrepareCalibrationInstrument`: after common
   measure/header/context checks, route explicitly requested executable
   `ScriptMC` Price through a generic admission branch before `Payments` and
   native `Payoff` matching. Keep all non-MC and affine routes unchanged.
3. That branch reuses `ForStatements`/`Guard` over the whole bound AST for
   receiver/history/reset checks, permits scalar non-receiver assignments,
   retains AtEvent/currency/route/Gaussian/sampling/opt-in checks and rejects
   EXERCISE, vectors and unsupported observations. For the initial hard AAD
   scope, reject IF/comparison controls rather than mix hard residuals and fuzzy
   derivatives. Execution still goes through `PrepareBoundScript`, which owns
   real FIX/model/history validation; no second compiler or bare-Product bypass.
4. Return generic ScriptMC capability/rule `dal.gaussian-script-price/1`, with
   the derived tag, empty coefficient cashflows and actual AAD/None method.
   Existing `RunView` already executes the original bound source. Generic Price
   does not acquire a deterministic/fast or affine capability.
5. Extend `DescriptorContextIdentity` for that tag to include the actual Libor
   day-basis convention (`RateContextIdentity`), and advance only the relevant
   generic compiler/rule identity token. Preserve accepted Deposit, BondOption,
   Caplet and Swaption fast rules. Native-template matches may remain available
   through their existing fast APIs.
6. Add dedicated general-Price/average, reset/extra receiver, early-exercise,
   hard-branch, currency/fixing, convention-change and binding/candidate cache
   tests in `test_calibration_preparation.cpp` after its write window is released.

The above is a precise implementation draft for coordination, not applied code
or an accepted new financial convention. The arithmetic candidate already uses
N/Notional and K/RateStrike, so it needs no `instrument.cpp` schema/role change,
new wire fields, enum regeneration or shared compiler modification. If the
DAL-406 variant changes while implementing its own descriptor, integrate this
append/branch on its actually merged master, not its unmerged product commits.
Only these necessary preparation files/tests need release; no claim is made
that DAL-406 must finish unrelated CI before a safe serial window can be granted.

Pending acceptance after release: implement generic admission; run the real
multi-observation arithmetic-average fit, independent path-payoff oracle,
AAD/CRN FD(h,h/2), binding/history/candidate freshness, single-fixing native
degeneracy with exactly matched accrual/payment semantics, independent
validation/held-out and frozen budget/refinement; measure that actual caller;
integrate the necessary guide/CHANGELOG text; coordinator-dispatched focused
review and exact-head CI. Existing vanilla evidence does not close these gaps.

## Proposed shared documentation text (not integrated)

Guide: “`CalibrateGSRScriptPrices` is an explicitly enabled core MC driver for
admitted Price observations and selected native Gaussian g coordinates. It
keeps fixed fit samples, independent validation and optional held-out ranges,
reports raw M×N Price/g derivatives and real execution counts, and separates
optimizer, fit, numerical and held-out outcomes. Prior/smoothing rows are
separate from data rows. FD and AAD report the actual method. General-script
admission and arithmetic-average numerical acceptance are pending; use the
existing native fast adapters for their accepted templates. Calibrated quote
risk is unavailable.”

CHANGELOG draft: “Add a core selected-g script MC calibration driver with
explicit budgets, raw row Jacobians, candidate structure reuse and separate
fit/validation/held-out diagnostics.” Publish only with the final accurate
admission/acceptance boundary after this blocked task is completed.
