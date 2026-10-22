# DAL-412: selected-g script MC calibration

## Delivery state and authority

**Full implementation and affected functional verification are complete under
coordinator comment `01a16211-de3f-765e-ad80-3aca8b597f90`.** The original PR #618
and checkout are retained. This record awaits focused review and exact-head CI;
no reviewer is dispatched by the author and DAL-413 remains downstream.

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

After DAL-406's clean delivery, this continuation owns only the authorized
preparation `.hpp/.cpp` and preparation-test window, plus its existing MC/driver
scope. Instrument, curve and shared compiler products remain untouched. The
authorized guide, CHANGELOG and own artifact-index integration is now included;
DAL-406's unpublished content is not imported. Existing guide links in the docs
indexes and CLAUDE already point to the same guide and need no new link.

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
General admission also performs a fresh uncompiled bound-script/model preflight.
This is repeated admission work, not a cached evaluator or a claim that all
parsing has disappeared. Preparation/refresh counters count logical outer plan
and admission calls; measured caller time includes the internal preflight.

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

The arithmetic-average protocol freezes N=100, K=.04, two future USD
6M Libor observations at 2027-10-21 and 2028-04-21 with weights .5/.5, and one
payment at 2028-10-21. Valuation=2026-10-21, flat log discount -.12 at
2029-10-21, one factor g=.02/H=1. The planned numerical protocol is the larger
589824-path reservation above, Price scale=.1, parameter scale=.01,
sigma=3, nu=nuG=.25, epsilonV=1e-10, epsilonG=1e-8, CRN FD h=1e-6/5e-7.
These choices precede any admitted candidate values. The serial continuation
uses the exact original flat curve and one constant g knot from the probe.
Before measuring the admitted candidate, the fit check is specified as two
Price observations: the original N=100 contract and its N=200 immutable rebind,
both scale .1, with independent path-oracle targets at g=.02. Initial g=.012,
one selected native g bounded [.001,.06]. Held-out targets use an independent
mathematical expectation and retain scale .1. Non-root checks use g=.016,
CRN h=1e-6/5e-7 and the existing raw-gradient comparison tolerance 2e-6.
Single-fixing degeneration keeps coupon accrual 1 and the actual delayed payment
to match the original cash amount exactly. No threshold is selected from results.
The independent held-out expectation conditions the second lognormal Libor on
the first Gaussian state under the payment-date measure, integrates the remaining
standard normal on [-10,10] by Simpson with 8192 intervals, and checks 16384
intervals at tolerance 1e-10. Its raw derivative uses the frozen h/h2 and records
their difference separately. It does not call a script matcher, GSR pricer or
model path generator. Pathwise oracle checks separately reconstruct the native
Gaussian endpoint/numeraire mapping from sequential MRG32 Precise coordinates.

The shared preparation window switches to DAL-412 after DAL-406's clean pushed
handoff at `e65420120bdd9d5461c44b77791ac96123849e7f`. This checkout remains on
merged-master f2dfa8e9 ancestry and imports no DAL-406 product. The bounded tag,
explicit generic admission and shared tests are authorized; instrument/curve/
compiler products stay outside this continuation. Shared publication writes
are authorized for DAL-412 after functional/numerical validation.

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
the seven relevant test translation units: driver, European/native calibration,
BondOption script, rate-option script, MC, preparation and Deposit caller. It is a dedicated
binary, not an assertion that the full `dal_cpp_tests` target was built/run.
The prior partial head passed 128 scoped checks. The complete continuation
first rebuilt and passed **146/146**: those same 128 plus 10 arithmetic-average,
six shared preparation and two execution-identity checks. The CI-discovered
Deposit admission regression below then expands the final affected set to
**168/168**, adding one preparation boundary case and all 21 existing Deposit
caller cases. All 14 accepted driver test bodies and their
helpers remain byte-identical. It covers tape cleanup/nested-recording recovery, exact labels, CRN path oracle,
binding/history, original fast/native/vol behavior, frozen numerical matrices,
and the new driver states. Final counts and immutable executable/source hashes
are recorded with the delivery. Full repository, facade, Windows and binding
matrices were not run; no example/build registration changed.

Build: GCC 15.2.0, CMake 4.2.3, Release, native AADET, Eigen 3.4.0, native-arch
OFF, public enabled, examples disabled; direct test/probe TUs use C++17/O2 while
the core archive uses the preset's O3. All five submodules are exactly at their
repository pins. Changed production TUs also pass GCC 14.3/15.2 syntax checks
with the repository's warning-clean categories/exclusions. The continuation's
preparation TU and both changed test TUs pass six GCC 14/15 warning-as-error
syntax checks. This is not a full GCC-14 library rebuild. CI and focused review
remain coordinator gates.
The subsequent compatibility fix also checks the MC production/test TUs under
both compilers. Final format/diff and 87 Markdown/link checks pass. All 2,853
inherited paths outside the 14 PR paths retain merged-master Git content;
2,848 are also raw-byte identical, with five existing CRLF checkout conversions.

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

Historical partial-head informational medians in microseconds (prepare/evaluate/Jacobian):
one row FD=167.619/284.759/1101.810, AAD=167.935/328.333/316.849;
two rows FD=348.221/570.054/2305.860, AAD=362.077/665.763/642.703.
One-row Jacobians execute FD 8 batches/512 paths versus AAD 2/128; two-row
Jacobians execute FD 16/1024 versus AAD 4/256. AAD tape peaks are 89/96 nodes,
1,966,080 capacity bytes. Process RSS high-water is 650208 KiB, including global
initialization and previous operations; it is not per-call allocated memory.
The final binary/source manifest and raw samples identify this build. No performance threshold or speedup
acceptance is asserted; these probes are outside the nine-target regression gate.
No basket optimization is required for correctness or proposed on this evidence.
Those old timings are not relabeled as current runs. The continuation builds
fresh one/two-row native controls and the real arithmetic-average caller with
the original full sampling reservation. Its exact commands, ten alternating
method rounds, batch/path/tape/RSS counts and source/executable hashes accompany
the delivery. Native controls substitute the actual pre-continuation preparation
TU compiled with the current extended header into the otherwise current archive;
this isolates the preparation delta and is not an untouched full baseline build.

Final continuation medians in microseconds (prepare/evaluate/Jacobian), with
the full 65536-fit-path reservation per observation:

| Rows | Method | Prepare | Evaluate   | Jacobian   | Jacobian batches | Jacobian paths |
|------|--------|---------|------------|------------|------------------|----------------|
| 1    | FD     | 438.097 | 15215.150  | 30801.600  | 16               | 131072         |
| 1    | AAD    | 427.657 | 64325.300  | 64589.300  | 8                | 65536          |
| 2    | FD     | 737.511 | 30474.700  | 60735.600  | 32               | 262144         |
| 2    | AAD    | 725.678 | 128110.500 | 127839.000 | 16               | 131072         |

For this single-parameter caller AAD uses fewer paths but takes longer than
double basket FD. No speedup is claimed. AAD peaks at 250 tape nodes and
1,966,080 capacity bytes; process RSS high water is 13,704 KiB with one DAL
thread, not per-call allocated memory. CPU 4 is pinned, compiler/core settings
match the functional build, and method order alternates over ten repetitions.

Native controls retain their batch/path counts and 89/96-node tape peaks.
Final one-row before/after prepare medians are FD 170.022/171.615 and AAD
166.747/168.797; two-row medians are FD 320.477/342.327 and AAD
336.882/337.822. Other evaluation/Jacobian changes are mixed small differences.
These isolated informational probes are outside the nine closed benchmarks;
neither a regression-gate pass nor a portable no-regression verdict is claimed.
Native pricing, optimizer, tape and RNG kernels are unchanged and outside the
measurement expansion. Public/binding/example/build callers are unchanged.

An intermediate probe exposed extra native preparation cost from source lookup.
Additional node-location lookup is now restricted to the generic boundary;
legacy source lookup remains on its original path. This is followed by
**45/45 directly affected preparation/average checks**, two final production-TU
warning checks, repeated legacy identity equality and refreshed caller binaries/
measurements. The original complete **144/144** scoped result remains valid for
the unchanged numerical paths. The final identity correction below then receives
the complete 146-case affected run and refreshed caller binaries. No unrelated
matrix is repeated. Intermediate and final timing snapshots are distinguished.

## Completed generic admission and serial integration

The original average RED fails at `Payments` with UnsupportedCalibrationProduct.
The continuation appends only the empty derived `CalibrationScriptMcDescriptor_`
tag. Explicit ScriptMC Price first runs the common complete receiver guard and
retains recognized native-template classification. A small shape selector only
routes known templates to the existing matchers and domain/parameter checks;
it supplies no financial descriptor or pricing authority. Other bounded scalar
Price programs receive `dal.gaussian-script-price/1`, AAD/None, no coefficient
cashflows and no affine/fast/deterministic capability. There is no MC fallback.

Generic admission checks exact context/model/header, opt-in, executable mrg32,
supported same-currency DF/Libor FIX and history snapshots. The full bound tree
rejects EXERCISE, vectors, SPOT, IF/comparison controls and ON settlement, and
passes the actual model and fixing requests through PrepareBoundScript. The
common receiver guard runs once before native or generic classification.
Only the generic compiler/rule tokens advance. Its context includes actual
Libor day basis; no key, role, wire field, enum or instrument schema changes.
The native Vanilla negative-target preflight remains effective.

A second behavioral RED proves a rejected EXERCISE in a merged event reported
row five instead of its true row six. Source lookup now uses that node's owned
location (and locations of the other admitted/rejected source-bearing nodes).
The same assertion passes after repair, including statement/date/report checks.
No shared compiler change is required. All six new preparation tests pass.

The isolated old/new preparation probes preserve all nine definition/binding/
instrument/compiler/context/fast-plan/MC-plan/mapping/quote fingerprints for
Deposit, BondOption, Caplet and Swaption. The baseline uses the actual 4d0d53ef
preparation with the current header layout and otherwise current archive; it
does not pretend to be an untouched full old build.

A further merged-baseline audit finds that the partial implementation had
included zero held-out counts in every MC execution hash. A new eight-case
golden regression from actual f2dfa8e9 preparation/MC source first fails, then
passes after omitting those fields when held-out is disabled. Enabled held-out
counts still contribute and remain refresh-stable. A separate four-family
probe now matches all nine fingerprints against actual merged f2 preparation
and MC source, compiled with the current headers and otherwise current archive.
This restores accepted P0 identities, rather than merely preserving the partial
PR's changed identities. No JCS/SHA algorithm, wire schema or financial inputs
change. The two new cases and all 27 prior MC cases pass, followed by 146/146
complete affected checks. Full before/after source and command provenance is
attached; this remains an isolated baseline substitution, not an old full build.

## Arithmetic-average evidence and honest outcomes

Ten new tests verify the original bound tree, compiled execution and pathwise
AAD against independent mathematical path/payoff/numeraire and g-derivative
oracles on every retained fit path. Non-root direct and full driver raw rows
match CRN FD(h,h/2); scaled penalties do not overwrite raw columns. Frozen
independent validation also matches a conditional-lognormal expectation and
its refined FD gradient. Simpson 8192/16384 refinement passes 1e-10. A six-time
refined event grid passes the same independent expectation/gradient budget;
its different dimension/mapping is not falsely called the same CRN path.

Starting at g=.012 recovers g=.019999999999999987. The original N=100 candidate
has fit mean .770221..., validation mean .772113..., SE .001646..., combined
error indicator .015935618758735004 <= .025 and gradient indicator .679506...
<= 2.5. Independent held-out passes scale .1. The actual optimizer reports
StepStalled/converged=false while fit and numerical/held-out flags pass; this
is retained as a separate outcome, not changed to convergence.

The predeclared second N=200 row keeps the same absolute scale .1. It recovers
the same g and held-out passes, but its combined error indicator .031871... >
.025, so the two-row result correctly reports numericalValidationPassed=false
and budgetExhausted for that row. The original harness assumption that every
such result must converge and pass every budget was incorrect; its raw failing
log remains attached. The committed checks assert the separate actual flags,
and the original 14 driver tests/helpers remain byte-identical. No frozen
sampling, scale, stopping default, tolerance or budget was enlarged to pass.

Single-fixing degeneration uses coupon accrual one, actual index accrual and
the same delayed payment as the original source. Its path values/mapping and
AAD agree with the native caplet source, while native Gaussian Price and its
refined FD gradient pass independent MC validation. History replacement by a
known literal, snapshot invalidation, N rebinding, g refresh/rebuild identity,
owned budget/numerical exceptions and post-failure tape reuse also pass.
Two initial reference fixtures omitted required gradient uncertainty/scaling
axes; those harness inputs were corrected without changing numerical limits.
Enum qualification, ON syntax, Matrix comparison and shell wildcard setup
corrections are not counted as product RED evidence.

## Documentation integration

The existing methodology guide now documents the bounded scalar admission,
arithmetic source, explicit budget configuration, selected-g entry, raw rows,
real work counts and separate outcome flags. CHANGELOG and the own artifact
index are synchronized. Existing Deposit/curve and Gaussian rate-option text
is retained, without importing DAL-406's unmerged capabilities. Existing docs
indexes already link this guide. Public/bindings, XCCY MC and calibrated quote
risk are not claimed. Pure Markdown work receives only static checks.

## Exact-head CI Deposit admission repair

The completed diagnostic job 119685645511 on head fc0c7231 failed the unchanged
`DepositScriptTest.TestSourceChangesReachFastAndBothScriptBackendsWithoutNativeEdits`:
both corrupted maturity payments were accepted as generic Price. This is a
product admission regression, not the separate checkout-cleanup warning.
The CI log is retained as `dal412-ci-extended-fc0.log`.

One new shared preparation test covers `N*(1+K*ALPHA)+1` and
`-N*(1+K*ALPHA)` after the existing `PAYS -N` opening payment. It and the exact
CI-failing caller test first fail locally, recorded in
`dal412-deposit-boundary-red.xml/log`. The minimal selector now reserves the
two-payment Deposit shape for existing native matching even when its maturity
formula is malformed. The full valid Deposit matcher and all identities remain
unchanged; one-payment signed general Price and arithmetic averages remain
admitted. No old test, instrument, compiler, Gaussian kernel or Brent code changes.

RED command after compiling the two affected test TUs against the old archive:

```sh
build/Release-linux/dal412-deposit-boundary-tests --gtest_filter='CalibrationPreparationTest.TestGeneralMcCannotBypassDepositPayoffChecks:DepositScriptTest.TestSourceChangesReachFastAndBothScriptBackendsWithoutNativeEdits'
```

GREEN rebuilds the affected core archive and runs all preparation and Deposit
caller tests: **57/57** pass. The final scoped script now includes the existing
Deposit test TU and passes **168/168**. This CI failure justifies that caller
expansion; unrelated matrices are not repeated. Warning/format/docs/complexity,
legacy identity probes and caller binaries/measurements are refreshed for the
selector delta. Raw build commands, XML, compiler logs and source hashes identify
each execution. Exact-head external CI remains separate from local results.
