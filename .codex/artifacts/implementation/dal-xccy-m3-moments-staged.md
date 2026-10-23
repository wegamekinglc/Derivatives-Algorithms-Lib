# DAL-422 Gaussian moments and staged basis implementation

The author implementation now includes final same-instrument tree/compiled
execution parity against accepted DAL-421, merged as
`f9b8044eac76cbe622e1443cf80806569ecbbf40`. It awaits its first complete M3
code review and independent V1 acceptance. M4 statistical estimation/partials
and M5 language facades remain separate work; this is not complete XCCY MC.
Synthetic recovery uses the independently accepted nonlinear qualification at
the frozen quote resolution. The original unsupported absolute-node assertion
and actual RED logs remain historical evidence.

## Authority and design

The implementation starts at `a5201cc95440a26a852dfa9db10cfc112d958060`,
containing accepted DAL-420 and DAL-409. Controlling inputs are the merged
DAL-410 model decision, especially sections 4–6 and 8; DAL-398's instrument,
observation and error contract; the parent research and incorporated expert
critique; the current methodology; and the DAL-408/409/M1 source and tests.
No unmerged sibling implementation is imported.

The complete admitted source/AST/date descriptor supplies the cashflows. A
sealed moment program owns the prepared immutable instrument and derived
terms. Outstanding coupons become exponential projection-bond ratios minus
constants; reset notionals multiply the appropriate realized FX; MTM exchanges
retain the source's new-minus-old sign. Each term includes payment-date
domestic discounting and, for foreign payments, payment-date FX conversion.
No native swap is retained beside the source.

The environment evaluates joint Gaussian exponential moments at all relevant
fixing/reset/payment dates. Cross-time covariance expands shifted endpoint and
integral kernels and reuses the accepted M1 closed-form kernel integrals,
without a transition factorization, simulation or trade-specific drift. Foreign
means retain the negative quanto drift; effective foreign discounting is Pf/B.
Historical values remain passive; at-V supplied FIX wins; future snapshot
values are ignored. Payment FX remains active. Settled payments are excluded.

A/B are accumulated separately, `Price=B+K*A`, and eligible par is `-B/A`.
Price does not inherit the native two-positive-annuities gate. All-expired Price
can be zero while par refuses. Zero vol is value supported; AAD at its boundary
is refused. Interior small positive sigma has no cutoff. Moment inputs expose
the model's ordered snapshot coordinates; the native map chains each snapshot
Jacobian back to its original curve coordinates. R/a and quote risk are not
advertised derivatives.

## Changed paths and explicit adapter choices

- `dal-cpp/dal/curve/xccygaussianmoments.hpp` and `.cpp`: sealed lowering,
  coefficient/value evaluation, native input/map access and owning diagnostics.
- `dal-cpp/dal/curve/xccygaussianmomentmath.hpp`: affine exponents, exact
  cross-time means/covariances, deterministic curve shifts and projection ratios.
- `dal-cpp/dal/curve/xccygaussianstaged.hpp` and `.cpp`: candidate snapshots and
  the complete active basis chain, including Pc_f, Phi_f and projection q.
- `dal-cpp/dal/curve/xccybasiscalibration.hpp` and `.cpp`: explicit optional
  Gaussian dynamics, candidate model ownership, provider selection, numerical
  context identities and accurate FastGaussian reports. The existing Fit,
  observation scales, raw axes, EXACT/APPROXIMATE criteria and legacy defaults
  remain in use.
- `dal-cpp/tests/curve/test_xccy_gaussian_moments.cpp`,
  `test_xccy_gaussian_staged.cpp` and `xccygaussianmomentfixture.hpp`: focused
  financial, ownership, adapter and refusal controls. Recovery-specific helpers
  retain the independent cashflow reference, node consistency, nonlinear secant
  qualification and negative controls.

Two bounded adapter details are necessary and need focused review:

1. The existing staged PWC basis has future knots with the first forward also
   applying before its first knot. A normalized model snapshot adds an explicit
   V anchor with that same first forward. Its duplicate coordinate is composed
   back into the original native map; no new free parameter is introduced and
   there is no extrapolation beyond the last declared knot.
2. The frozen three-free-log-basis-node fixture cannot use three old PWC forward
   knots at those same terminal dates: the final forward only applies after the
   last knot and is unobserved within the required coverage. The new explicit
   Gaussian path therefore additionally admits existing LOG_DISCOUNT geometry,
   selected by `basisParameterization_`; PWC remains the default. LOG_DISCOUNT
   without explicit Gaussian dynamics is rejected. Its native report units are
   Dimensionless. This is an additive core selection, with no solver rewrite or
   shared schema/binding/public guide change.

The shared adapter diff is limited to provider/layout selection, report and
candidate ownership. Native staged/joint solver bodies, RNG, model/preparation
hooks, M2 planner files and public bindings are untouched. M5 owns publication
and language access; this record does not publish a new facade capability.

## Deterministic numerical controls

The evidence bundle includes `moment_oracle.py`, `moment_probe.cpp`, qualified
date tables, source-derived term recipes, original and final logs and numeric
JSON. These are validation tooling, not a multiprecision production API.

The independent reference lists each signed cashflow from a frozen date table.
It calls neither the native XCCY pricer, the moment lowerer, M1 transition
builder nor Cholesky. Brownian kernels are integrated separately at 45/65/85
digits, with interval bisection and entry targets 1e-30/1e-45/1e-60. Every
reported quadrature error must meet its target. Separate 50/80-digit closed-form
replay consumes the emitted source-derived term recipes and carries forward
dual derivatives. Its indicator uses its own double/50/80 differences, never
the oracle discrepancy.

The frozen ordinary budgets remain A/B/Price=1e-10 and par=1e-12, with floors
1e-11 and 1e-13 respectively. Oracle and fast numerical indicators each must
be at most one quarter of the relevant budget, and the direct discrepancy must
be within their sum plus the floor. Selected raw derivatives use the frozen
native parameter scales and the corresponding divided budgets. Oracle gradient
indicators include both precision refinement and h/h2/h4 refinement.

The original two-point FD diagnostic failed the reset-par spot gradient's
indicator, despite a tiny fast/reference difference: its truncation indicator
was about 1.1144e-10 against a 2.5e-11 indicator allowance. It is retained in
`moment-gate-two-point-failure.log` and `moment_oracle_two_point.py`. The
diagnostic was improved to the fourth-order centered stencil, uniformly, with
the same h/h2/h4, scales, fixtures and budgets. No threshold was relaxed.
For small sigma, deterministic halving keeps the entire stencil interior.

Coverage in the direct control includes all three modes, both spread legs,
both directions, zero/nonzero dynamics, nonflat curves and volatility buckets,
the three synthetic maturities, small sigma, zero a and FX-only value limits.
Each selected case reports A/B/Price/par and applicable raw spot, curve,
projection, basis and rate/FX-sigma partials. History/expired unit tests also
use independent outstanding-cashflow formulas; they are not relabeled as the
whole high-precision history matrix or as MC parity.

The payment-lag mutant replaces only Cov(q,w(P)) with Cov(q,w(T)). On the base
fixed domestic-spread receive view its Price difference is approximately
+2.3401625053e-5 USD, which fails the unchanged 1e-10 direct gate. The sign
follows the selected source orientation. MCSE is not used in this gate.

Schedule qualification precedes fitting. The base fixing table is USD
2027-04-30/2027-08-02 and EUR 2027-04-30. An initial guessed EUR 2027-04-29
reference was corrected during calendar qualification before oracle comparison;
the original and qualified control hashes are retained. The named third
maturity 2028-11-06 is rejected by canonical import. The admitted replacement
2028-11-04 was frozen before fit, with foreign payment 2028-11-07 and basis
coverage through 2028-11-08. No geometry admission was weakened.

## Synthetic recovery qualification

The three maturities are 2027-11-04, 2028-05-04 and qualified 2028-11-04.
The three free logB dates remain 2027-11-08, 2028-05-08 and 2028-11-08;
target logB=-.003*t, initial=target+.001, and observation errorScale=1e-4.
Quotes come from independent moments at the frozen dynamics. EXACT accepts
scaled infinity <=1; neither the quote scale nor solver criterion was changed.

At the original checkpoint, both FD and AAD fits returned within that quote
criterion, but the author's additional 1e-5 absolute node-recovery assertion
failed at the final node.
The AAD candidate is approximately
(-.0031477624753, -.0046486525102, -.0063053062723); the third target is
-.0061561643836, a discrepancy of about 1.4914e-4. Raw quote residuals are
approximately (3.6250e-7, 3.9819e-7, 3.8588e-7), and scaled infinity is .003982.
The raw Jacobian has rank 3, singular values approximately
(2.515259, .661692, .000721622), condition 3485.56 and determinant .00120101.
This is not an underdetermined fit or a failed raw Jacobian.

The independent critic's accepted qualification establishes that the frozen
quote acceptance domain cannot imply that extra absolute-node promise. With
all inputs unchanged, target+(0,0,.001) has scaled quote infinity .234671 and
still passes EXACT. Full local rank does not make the quote acceptance domain
a global parameter error bound. ErrorScale is the solver's acceptance scale,
not rounding of synthetic quotes or a probabilistic noise model.

The revised test preserves every dated node's target, initial, returned value
and error, verifies improvement from initial on this fixture, and checks the
returned curve, owned model snapshot and native column labels against those
parameters. It independently reproduces target quotes, returned values and raw
Jacobian. For e=returned-target, it integrates the independent Jacobian along
that segment with composite Simpson rules at 4/8/16 intervals. The coupling
Jbar*e=f_reference(returned)-f_reference(target) must satisfy the original quote
scales, with separately controlled refinement and the existing deterministic
par budget/floor. No per-node upper bound is qualified without a local-domain
and curvature or interval-inversion certificate.

The canonical C++ regression uses signed exponential cashflow coefficients
computed from the checkpoint's independent explicit-cashflow oracle, without
calling the production lowerer, descriptor terms, transition or solver.
`xccy_gaussian_recovery_reference.py` pins both original input hashes, generates
at 65/85 digits and requires identical 30-digit coefficient output. The supplied
original oracle and case JSON are in the evidence bundle. After clang-format,
regeneration must reproduce `xccygaussianrecoverycashflows.hpp` exactly.
Basis enters only the deterministic log-linear shifts, so each cashflow's
exponent load is constant; the frozen coefficient includes its independent
payment/reset covariance and the original normalized source ratio.
These are three-row test data, not a second product/pricing authority.

Negative controls accept the weak-direction alternative at quote resolution
while refusing its 1e-5 label; reject the unchanged initial (scaled infinity
19.8417), swapped node dates/columns, mixed parameters/curve/value/Jacobian and
an erased weak column. Raw determinant acceptance remains >1e-4, and direct
par and native-partial reference checks remain 1e-13 and 1e-10 respectively.
The latter is the existing par floor divided by the .001 native scale.
No hidden extra Newton solve, optimizer, curve correction or internal residual
scale change was inserted. The old FD/AAD failure and real CI logs are retained.

At the recovery repair's dependency read, PR628 was still open. That repair
alone did not complete M3. The subsequent accepted merge and actual execution
parity are documented below; no unmerged M2 branch was imported. Neither author
result replaces the first complete code review or independent V1 acceptance.

## Verification and identity

RED/GREEN logs include the missing moment API; missing explicit staged fields;
the hard-coded canonical tenor route and correction; logDF report unit and
correction; and par diagnostic ownership and correction. The recovery assertion
was subsequently replaced by the explicitly authorized qualification above;
all frozen direct financial and solver gates remain. Fixture/harness errors (initial duplicate instrument IDs,
wrong spread-leg quote in the ownership probe, shell glob and compile/include
errors) are preserved separately from product RED and accepted results.

The final core build uses GCC 15 Release/O3, native AAD, Eigen, C++17, native
architecture OFF, public/examples/portable Excel OFF. The custom focused runner
uses the repository's initialized core test main and freshly links that archive.
It covers the new cases, all 15 old instrument-adapter cases and affected
legacy basis/joint Jacobian suites. The one node-recovery failure is reported
as a failure, not a successful test or an expected-pass waiver.

The evidence bundle records exact commands, exit codes, compiler/dependency
versions, CMake cache/compile/link commands, all changed source SHA256 values
and executable/archive SHA256 values. GCC14/15 syntax checks apply the existing
warning policy with Werror to the affected TUs. CCN<=8, formatting, generated
drift, Markdown and diff checks are separately recorded. No build/example files
change, so no unrelated installation or component-example matrix is claimed.

Performance selection maps only the new analytic/staged caller to a three-row,
three-node MTM basket, separating prepare, Evaluate AAD and solve. Three warmups
and ten samples are retained. Initial minima are about 1.5 ms / 3 ms / 9 ms;
final samples have their own binary identity. These are informational host
measurements, not a speedup claim or a formal regression-gate result. The old
P0/P3, full benchmark/statistical matrices and native Windows/XLL/workbook
execution are not rerun or claimed. The PR's one CI snapshot is handed off
without watching or waiting for external CI.

Original checkpoint commands/results (the evidence contains the complete compiler
and linker argument vectors):

```text
cmake --build build/m3 --target dal_cpp -j4
  PASS: public-disabled Release core archive, final source
build/m3/moment-tests --gtest_filter='XccyGaussian*Test.*:XccyInstrumentCalibrationTest.*:XccyBasisJacobianTest.*:XccyJointJacobianTest.*'
  49 executed: 48 PASS, 1 FAIL (the retained node-recovery assertion)
build/m3/moment-probe ../final-moment-cases.json
python ../moment_oracle.py ../final-moment-cases.json ../final-moment-gate.json
  PASS: 36 cases, 1,356 derivative-quantity comparisons
  max value discrepancy 5.203e-14; max gradient discrepancy 7.590e-14
  max value eOracle 1.378e-43; max value eFastNumeric 2.376e-13
build/m3/staged-probe ../final-staged-result.json
  Quote fit accepted; rank 3; node recovery still fails its stronger control
  final prepare/Evaluate-AAD/solve minima 1564/3006/9594 microseconds
python ../verify_m3.py
  PASS: 10 GCC14/15 affected-TU warning-policy checks
cmake --build build/m3 --target dal_check_generated -j4
  PASS: Machinist wrote 0 files; no generated drift
python -m lizard -C 8 -L 1000 -a 100 <affected C++ files>
  PASS: no threshold exceeded, including the shared adapter
clang-format --dry-run -Werror <new C++ files>
  PASS; shared-file changed ranges formatted separately
python .github/scripts/check_docs.py
  PASS: 108 Markdown files
git diff --check
  PASS
```

Generated checking refreshed unchanged dependency timestamps and caused a wider
core rebuild. The moment and staged probes were relinked and rerun **after**
that rebuild completed. Earlier overlapping probe runs are explicitly saved as
prefinal data and are not used as exact-final executable evidence. No required
run-owned process is left active at handoff. Final publication SHA/CI state are
in the issue comment and attached identity manifest, not inferred from this
record's own Git commit.

## Recovery repair verification and reuse

The original executable reproduces the exact third-node RED
`.00014914188869427029 > 1e-5`. Rebuilding only the affected staged caller with
the unchanged core archive gives 6/6 staged tests: the four existing cases,
including the qualified original case, and two new negative-control cases.
This does not relabel the old 48/49 result as an old all-green run.

A fresh one-fit caller returns exactly the original parameter/value/residual/
Jacobian arrays, model/plan identities and Fit counts (2 evaluations,
1 gradient). Diagnostic attempts remain 1/1. No timing or extra diagnostic
solve is inserted into that call. The local independent 65/85-digit reference
checks all three rows, averaged Jacobians and their refinement. Its secant
error is about 1.23e-27 and indicator 1.18e-24; the ordinary C++ caller reports
about 1.07e-19 and 1.68e-23 respectively. The single endpoint Jacobian would
miss by 3.49e-9, above the unchanged 1e-12 par budget.

GCC14/15 warning-policy syntax checks cover the changed test and all its new
headers. CCN<=8, formatting, coefficient regeneration and Markdown/diff checks
apply to the repair. A validation-only copy replacing the reference arithmetic
and literals with double also passes the same six cases; this tests the
53-bit arithmetic boundary, not a Windows compiler or additional distinct tests.
The initial validation harness run omitted a requested
Solve Jacobian; its failure is retained separately and corrected by explicitly
requesting derivatives. An initial Python check likewise assumed a derivative
on a genuinely passive column; the final check records zero for that column.
Neither harness error is product RED or a relaxed financial test.

Only test/validation helpers and this active record change. The fixture,
production sources, solver/scales/budgets, direct numerical thresholds,
dependencies and core archive match the original checkpoint. Thus its unchanged
direct moment/gradient, compatibility, ownership and informational performance
evidence retains its original scope and exact binary identity. This repair does
not rerun the 49-case scope, 36-case/1,356-gradient oracle matrix, MC/statistical,
performance, sanitizer or component/install matrices. It claims Linux caller
execution, not native Windows/XLL/workbook execution or final M3 parity.

## Final same-source execution integration

Continuation starts from `cf966f36deb892630fb428263b5bd9b391c7b479` on the
original branch and PR629. GitHub and Multica both confirm PR628 merged at
2026-10-23 09:46:33 UTC; fetched master is its squash commit
`f9b8044eac76cbe622e1443cf80806569ecbbf40`. A normal merge adds exactly its
seven accepted paths, unchanged. M1, the M3 production code, shared adapter,
solver, fixture, scales, financial admission, RNG and bindings are unchanged.
The resumed shell lacked a Git identity; the merge/commit commands explicitly
reuse Cheng Li's name/email from both existing commits, without changing shared
or global configuration.

Four dedicated test/tool files are added:

- `dal-cpp/tests/curve/test_xccy_gaussian_execution.cpp`: seven integration
  tests, using the existing initialized core test main.
- `dal-cpp/tests/curve/xccygaussianexecutionfixture.hpp`: explicit signed
  cashflow reference, independent scalar Brownian integrals, and a pathwise
  reader of the actual lowered terms. The reference lists the qualified
  two-domestic/one-foreign coupon table and principal/MTM cashflows; it calls
  neither production transition/moment kernels nor a native pricer.
- `dal-cpp/tests/curve/xccygaussianexecutionreference.hpp`: 30 retained
  independent expectation controls, including all 24 base combinations and
  six nonflat/bucketed cases. Each quantity retains the oracle value/indicator,
  80-digit fast replay and 50/80 refinement difference.
- `dal-cpp/tests/curve/xccy_gaussian_execution_reference.py`: deterministic
  extraction from the original qualified gate JSON, pinned to SHA256
  `0cabea620823b1b9c061ccae69df61251d9d41b3b0cd4f90baba17791e680d97`.
  This extracts existing evidence; it does not rerun or replace its oracle.

Each base/nonflat instrument is imported once, then the same immutable handle
and preparation context feed the M2 planner and M3 lowerer. Three fixed normal
vectors drive the accepted model's exact state path. The tree and compiled
receivers agree with each independently listed payment; lowered terms agree
by payment date, so offsetting coupon/principal errors cannot pass only by
total PV. The independent path reference uses 32/64-panel composite Simpson
Brownian integrals, split at each volatility bucket. Their pathwise refinements
and receiver discrepancies use the unchanged 1e-10 USD gate.

Fresh M3 A/B/Price/par are compared directly with the retained independent
Brownian expectation controls. The oracle indicator retains its original
45/65/85-digit refinement plus the bound for representing the retained decimal
in the caller's arithmetic. The fresh fast indicator is four times the larger
of the new double-to-80-digit-replay difference and the stored 50/80 refinement,
including that representation bound. The original independent budgets/floors
and separate quarter-budget checks remain: A/B/Price 1e-10/1e-11, par
1e-12/1e-13. The extraction retains 31 significant digits, with conversion
covered conservatively by the arithmetic representation bound. Fast/oracle
discrepancy is never used as either indicator. Ordinary 53-bit `long double`
platforms use their actual epsilon in this bound.

With all sigmas zero, actual tree/compiled Price equals M3 expectation. Immutable
K=0 and K=1 bindings additionally extract script B and A and their signed par;
these agree with moments. Nonzero paths are checked as realized cashflows,
not equated with expectations. No sampling stream, MCSE, phase reservation,
MC optimizer or statistical acceptance is introduced by this completion.

The remaining integration tests cover all principal exchange switches and K/N
rebinding; strict-past missing refusals; supplied/active at-V rate and reset FX;
future snapshot entries ignored; omitted strict-past history frozen against
later global changes; conflicting same-time globals excluded; one-leg-settled
and expired Price/par boundaries and independent legacy refusal. The existing
three-maturity fit is run once in the new test and its actual returned model
drives each original source through both receivers and the lowerer's realized
terms. Its par reproduces the independent recovery reference and owned report.
The accepted secant qualification, node/date/raw-column checks and negative
controls remain unchanged.

## Execution parity verification and reuse

The incremental public-disabled Release core build compiles only the accepted
M2 source and relinks `libdal_cpp.a`. The new caller is freshly linked against
that archive. Its final focused run executes **13 distinct tests: 7 new parity
and 6 directly affected staged tests, all passing**. This is not a rerun or
relabeling of the original 48/49 checkpoint. The new tests' JSON properties
retain all 120 actual A/B/Price/par values and independently computed indicators.
Their largest direct discrepancy is 1.764e-14; the largest fresh fast indicator
is 9.840e-14 and represented-oracle indicator 5.988e-18. Each is checked in its
own quantity's unit and against its own unchanged budget, not a pooled bound.

Three validation-only copies give the required sensitivity controls. Replacing
reset-date FX with payment-date FX, reversing MTM new-minus-old, or replacing
delayed foreign-payment FX with index-end FX each compiles and then fails the
focused parity test with exit 1. The unmodified caller passes. These are test
helper mutations, not product defects or production changes. The original
independent expectation lag-covariance mutant remains separate evidence.

The first principal-switch harness run accidentally inherited the importer's
foreign-spread default while the explicit reference requested domestic spread.
Its failure is retained and the fixture explicitly selects domestic spread;
no assertion or numerical threshold changes. An initial reference regeneration
comparison used clang-format's default style outside the repository. It is
retained as a tooling failure and corrected by selecting this repository's
`.clang-format` explicitly. Neither is presented as a product RED.

Final commands and exact argument vectors are in the execution evidence bundle:

```text
cmake --build build/m3 --target dal_cpp -j4
  PASS: accepted M2 source added to the existing public-disabled core
python ../execution-parity/verify_execution.py
  Three compiled mutants fail the intended assertion (exit 1)
python ../execution-parity/verify_execution.py --skip-mutants
  Fresh combined caller: 13/13 PASS; GCC14/15 Werror PASS
  CCN<=8, format, pinned reference regeneration and diff PASS
python .github/scripts/check_docs.py
  PASS: current Markdown set; record checked after its final edit
```

The bundle records the input head, merge parent, exact final head, unchanged
production hashes, all seven upstream byte comparisons, dependency/configuration
and compile-command comparisons, fresh archive/caller SHA256 and full logs.
The previous archive identity remains attached to its original numerical
evidence; adding M2 changes the current archive identity, so it is not called
the old binary. Source/dependency/configuration applicability and the new
integration/staged caller justify reuse of unchanged gradient, compatibility,
parameter-recovery and informational performance evidence within their original
scope. The new 120 value checks are fresh execution against retained references,
not 120 new high-precision oracle integrations.

No full 49/38/36-case/1,356-gradient, P0/P3, stochastic/performance, sanitizer,
install/component or native Windows/XLL/workbook matrix is repeated. There are
no build/example/enum changes beyond the accepted upstream merge; their content
identity is checked without regenerating accepted upstream output. Local timing
is the new caller's observed test time, not a product performance claim. The
final exact-head CI snapshot is handed off once without waiting. Complete M3
code review and V1 remain coordinator-owned; downstream tasks are not started.
