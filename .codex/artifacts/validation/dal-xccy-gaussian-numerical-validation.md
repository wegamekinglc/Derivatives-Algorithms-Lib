# DAL-425 independent XCCY numerical acceptance

## Current decision

Core acceptance checkpoint; **frozen numerical acceptance fails**. This is a
test/tooling PR, with no production repair. Price/par parity passes on the
Validation x1/x2/x4 prefixes, but precision and combined acceptance fail.
Large-sample native-gradient qualification now exists: all nine coordinates
pass parity, eight pass precision/combined, and FX sigma fails both budgets
even at x4. Estimates, actual batches, failures and identities are retained.

This checkpoint is deliberately not final V1 acceptance. DAL-424 was still
in progress without a linked PR when checked. Its actual merged public C++,
Python and Excel boundaries must be tested before the issue can enter final
review. The mixed-dimension held-out basket refusal, remaining statistical
combinations and boundary coverage prevent calling the core matrix complete. Parent
DAL-396 must remain in progress. No model, financial convention, scale, epsilon,
path reservation or variance-reduction policy was changed to obtain a pass.

## Authority and applicability

Product base: `4dbdff636bcd92d2a236abe644a3e0c73b9c208a`, actual merged
DAL-423 / PR630, including M1/M2/M3 and DAL-409. Authority is DAL-410 section 8,
SHA256 `c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`.
The parent issue, current issue/threads and active sibling runs were read.
DAL-424's unmerged tree was not imported. Existing M1-M4 implementation records
establish scope and historical failures; they are not this tester's execution.
Old GSR/P0/P3 numerical and performance matrices were not repeated or borrowed
as acceptance of the new model. No production, public docs, examples, build,
protocol, solver or RNG file is changed.

Fresh configuration is Linux/WSL2 x86_64, GCC 14.3.0, Release/O3, C++17,
static native AADET, native-arch off, public/examples/portable Excel off.
Eigen's GitLab remote failed twice with server-load errors. The supported
built-in matrix kernel is explicitly selected; this is not evidence for an
Eigen or Windows configuration. Google Test `d72f9c8a`, Machinist `e76b0ef2`
and RapidJSON `ea152ded` use their repository pins. mpmath is 1.3.0, Python
3.13.9. The attachment manifest records all source blobs, dependencies, cache,
compile database, scripts, archives, executables and raw artifacts.

The large MC runs use capture executable SHA256
`90c3625ecbd2fc38df99ec01fef4452683134dd40862d6d22aae238ebcdaad20`
and core archive
`cd610b9926e54f234642e758cba12963fd0b7d7a2a8b3d46ace2001b834c3ee7`.
Later capture-only changes add history/kernel/CRN exports, fix the tree selector,
and assert calendar metadata. They do not alter the MC branch or core archive.
Their separate executable identities are in the manifest; no claim is made that
the earlier executable contained these later checks. The final regenerated
reference header is compared byte for byte after formatting.
The exact initial driver source is retained in the evidence with its pre-run
SHA256 `5a626b98407371ff2a234d567efadaa286cc8cd0a229f4ad8ddf9f5635c69c26`;
its `Mc` function is byte-identical to the final helper's function.

## Independent references and direct gates

The new financial oracle uses explicit cashflows and scalar Brownian integrals;
it never reads exporter/matcher/lowerer terms when building its financial answer.
Production terms are used only by a separate wide numerical diagnostic. Native
curve bumps apply the complete exported snapshot Jacobian, including basis
through foreign effective discounting, projection shifts and drift calibration.
This is raw native risk, not calibrated quote risk.

The independent calendar/index table freezes the actual synthetic fixture's
empty holiday centers (weekends excluded), ACT360, two-day rate fixing lag,
USD 3M and EUR 6M. It does not assume London holidays. Dates are independently
resolved and checked before valuation: start 2027-05-04; USD fixings 2027-04-30
and 2027-08-02; coupon ends 2027-08-04 and 2027-11-04; foreign payment
2027-11-08. The no-lag variant pays on 2027-11-04. Both directions, both spread
legs and FIXED/RESETTABLE/MARK_TO_MARKET are covered under flat, zero-vol,
nonflat/bucketed, no-lag and zero-a inputs: **60 cases, 240 value gates**.

Kernel integrations use 45/65/85 decimal digits, native-entry targets
1e-30/1e-45/1e-60 and 1/2/4 subdivisions per knot interval. Wider closed-form
term replay uses 50/80 digits. Each direct gate separately checks oracle and
fast-numeric quarter budgets, then the combined deterministic inequality.
The maximum value oracle indicator is 7.224e-43; maximum fast indicator is
1.205e-13. Maximum absolute A/B/Price/par errors are respectively
3.358e-15, 2.290e-14, 2.281e-14 and 4.186e-16.

For the ordinary MTM fixture and its nonflat/bucketed counterpart, **39 native
axes / 156 A/B/Price/par gradient gates pass**. Native scales and four-point
h/h2/h4 stencils retain section 8's steps and tolerances. Independent FD uses
all three precision refinements; wide fast AD is differentiated at 50/80 digits.
Maximum AD/reference discrepancy is 3.803e-14. No unsupported a/R/quote-risk
gradient is advertised.

Five direct covariance/factorization controls cover ordinary, sigma_d=1e-8,
zero, FX-only rank and a=0 cases. Every C and L*L^T entry is normalized by its
own independent diagonal scale; exact zero rows stay zero. Maximum normalized
covariance/reconstruction errors are 8.184e-17 / 1.356e-16. The positive-pivot
truncation mutant produces normalized error .1999925 and fails 1e-12.
Independent domestic/foreign unit martingales and Qd-versus-Qf foreign-coupon
calculations pass for flat and nonflat/bucketed inputs; coupon differences
are below 8e-85 USD at the reference scale.

Six additional supplied-history cases at 2027-08-04, 2027-11-05 and
2027-11-09 check outstanding A/B/Price with separate direct numerical indicators,
both spread legs, signed annuity/refusal and legacy refusal. Maximum cashflow
error is 7.583e-15 and fast indicator 4.966e-14. The supplied at-V reset is 1.17,
while active payment spot remains 1.10. All-expired Price is zero and par is
refused; one-leg-settled domestic-spread par is refused. These six references
did not establish an independent historical derivative matrix at the original
`59714aee` checkpoint. The R2 extension below supplies those derivatives.

## Running existing tests, authoring tests, repairing failures

Fresh targeted existing tests pass **96/96**: 63 selected model/data/moment/
staged/execution/MC/boundary checks plus 33 exact-model/observation checks.
They exercise existing snapshot isolation, supplied/empty/omitted/future
fixings, principal and binding mutations, owning failure/candidate lifetime,
last-complete candidate, bounds/rank/zero-vol derivative refusals, tape cleanup,
phase overrun/overflow, wrong-origin Validation and the original narrow-coupon
regressions. Their XMLs and actual filters are retained. This is not a full
Linux, public, Python, native Windows, XLL or workbook run.

Three new `XccyAcceptanceTest` cases pass. They protect the 60 independent
calendar/cashflow references, 39 native gradient references, and complete
five-slot/three-driver mapping with exact scalar intervals and reverse-order
Fit/Validation/HeldOut traces. The generator and small numerical tools are
private test helpers and require focused numerical/code review.

No product behavior was repaired. Initial harness include/JSON-construction
errors were corrected before execution. A later harness check found that the
default compiled selector had been used on both sides of the intended tree/
compiled comparison. The final capture explicitly selects tree first and
compiled second; all 60 cases pass and the captured numeric records remain
unchanged. Earlier captures alone did not prove that comparison.

The actual binary64 CRN h/h2/h4 capture preserves **five of nine mean-gradient
rows failing the 1e-8 floor** at at least one step; maximum batch discrepancy
is 6.50e-8. The wider independent scalar-path reference (the existing accepted
Brownian/Cholesky cashflow helper, re-executed here) passes all nine at 50/80
digits and all three steps: max AAD discrepancy 1.756e-14, max FD-refinement
indicator 1.777e-19. The raw subtraction RED is retained, not called a product
repair or hidden by a larger step/tolerance. This small-path qualification is
separate from the large-sample gradient run below.

Negative controls remove quanto, reverse Pf/B and remove only payment-lag
covariance. On the actual normalized FIXED domestic-spread receive fixture,
Price deltas are +.00389795235964922, -.331322468954606 and
+.0000234016250532458 USD. Each fails the 1e-10 direct gate; lag-only error
is below the MC .0025 budget. Exact extra-normal traces fail the expected
coordinate/normal comparison. The prequalified wrong-gradient-SE control
incorrectly passes precision when Price dispersion is substituted, while the
correct gradient dispersion fails. These mutants do not modify production.

## Frozen sampling and statistical verdict

The original first-execution freeze file records the financial oracle and
capture code hashes before capture. Subsequent metadata/history/CRN harness
changes retain intermediate hashes and comparisons, with final code re-frozen
before its fresh independent run. Sampling formulas/scales come from the
pre-existing section 8; statistical tooling was completed after the first
Validation dump. This is not a claim of a blinded discovery experiment.
HeldOut is not used to change any decision rule or input.

All long runs use MRG32 seed 1024, Precise, no bridge, compiled script, D=30,
five innovations per interval and nine actual pathwise AAD native coordinates.
Fit reserves [0,262144), Validation [262144,1310720), HeldOut
[1310720,2359296). Every phase has 16 equal batches. x1/x2/x4 select
4096/8192/16384 paths per Fit batch and 16384/32768/65536 per Validation/HeldOut
batch. Refinements intentionally replay nested prefixes; they are not new
independent data. The maximum distinct reservation is 2,359,296 paths per
mapping, not an unreported expanded budget. Each path occupies [pD,(p+1)D).

The independent statistics tool reconstructs the entire (B,A,dB,dA) batch
covariance and projected SE. Par is -mean(B)/mean(A), with signed denominator
confidence, and its gradient SE uses all four coordinates for each parameter.
Price/spread scales remain .01/1e-4, k=3, nu=nuG=.25, epsilon=1e-10/1e-12,
gradient epsilon 1e-8. Fit/HeldOut flags and reservations are independently
checked; neither is relabelled or sent to `ValidateXccyMc`.

Validation Price means are -3.687756213580768, -3.735062508394368 and
-3.719869698700487 for x1/x2/x4, with SE .0584723458732498,
.0366120653334100 and .0197249754980186. Independent price is
-3.722575420314313. At x4, Price 3SE=.0591749264940557 > .0025 USD;
par 3SE=.00107235954235181 > .000025. Parity passes; precision, combined and
overall numerics fail at every prefix. `--strict` exits 1 after retaining the
complete report. The earlier M4 failure is independently reproduced.

At x4 the FX-sigma Price gradient is .00548484723025528 versus independent
-.000309750824947474, SE=.163717832532175; 3SE=.491153497596526 > .25.
Its par gradient is .000105529984004922 versus .00000767014831480103,
SE=.00296737821363460; 3SE=.00890213464090380 > .0025.
All gradient parity checks pass; this coordinate fails precision and combined
at x1/x2/x4. Eight other native axes pass both Price and par gradient budgets.
All estimates remain available and budget-exhausted/numerics=false is preserved.
Prefix mean movement is reported as a refinement observation; no independent
SE is invented for overlapping prefixes and no ratio-bias pass is inferred.

All three phases' x1/x2/x4 runs completed in this foreground run. HeldOut x4
Price is -3.690136443542591, SE=.0194326016495856; par is
-.0664667999339453, SE=.000352233264297293. Both pass parity and fail precision
and combined. The FX-sigma gradient also fails precision/combined. All nine
phase/prefix reports retain numerics=false. Fit x1 additionally fails the spot
Price-gradient combined budget; this is kept separate from Validation and
does not change any rule. Fit/HeldOut are independent phase audits, not
successful calls to the Validation-only product validator.

## R2: complexity repair and independent core extensions

The original `59714aee` remains a distinct review checkpoint. R2 changes only
acceptance tests, private fixtures/tools and this record. It addresses all nine
reported complexity sites by extracting named operations, retaining formulas,
arguments, exceptions, thresholds and original RED/GREEN results. A second
McCabe check also exposed the nested kernel helper's aggregate complexity;
extracting its functions preserves the order of numerical operations. Local
Lizard reports maximum CCN 8 across the acceptance files; Ruff F/C901 with
maximum complexity 8 passes. These local checks do not claim a new Codacy
server verdict.

A focused helper audit found that `zip` silently accepted a missing covariance
row or a shortened row. Both malformed-data probes first reproduced RED; exact
outer and row dimensions now reject them, while the complete original matrix
passes. This repairs validation-tool admission, not product statistics or any
numerical threshold. Original complete-report equality is checked again.

The original sixty-case JSON capture is byte-identical. Re-executed original
oracle values, 156 derivative gates, refinement values and three mutants are
identical, excluding changed script hashes. Kernel/Qf/history controls and all
nine original statistical reports compare exactly. The statistical strict
command still exits 1. The original `Mc` function is byte-identical to the
pre-run driver, and the old executable and archive hashes above remain valid.
R2 does not re-run or relabel those nine long samples. The new matrix uses a
separately named `xccyacceptancecapture-r2-final` executable; its hash and compile
command are in the R2 evidence. No old executable was overwritten.

### Three-maturity qualification and staged fit

The named `2028-11-06` maturity fails actual admission with
`UnsupportedCalibrationProduct`, field `/import/rate`. An initial independent
calendar check also rejected the mismatch, before any new fit or MC. Both RED
records remain in the evidence. DAL's canonical period maturity is the
unadjusted calendar-month date, including Saturday `2028-11-04`. Section 8's
fixture-qualification rule permits correcting the invalid fixture; the matcher
and financial rules remain unchanged.

The supported independent date table freezes maturities `2027-11-04`,
`2028-05-04`, `2028-11-04`: respectively 2/4/6 USD periods and 1/2/3 EUR periods,
with final foreign payments `2027-11-08`, `2028-05-08`, `2028-11-07`.
The three free basis knots remain `2027-11-08`, `2028-05-08`, `2028-11-08`,
anchored at V, target logB=-.003*t and initial target+.001 per free node.
Independent cashflows and 45/65/85-digit Brownian integration generate par
quotes `-.06705259701832679`, `-.054512806033648581`,
`-.049912191545990019`. No production expectation, native quote generator or
moment-term replay supplies these quotes. Geometry, target, initial curve and
quote refinements were captured before observing the fit or starting new MC.

The first staged helper attempt retained an APPROXIMATE admission failure for
missing explicit `/solver/fit_weight`. Its corrected configuration uses the
existing staged test's lambda=1e8 and eta=.5; it does not change quote scales or
acceptance thresholds. EXACT and APPROXIMATE both preserve original row IDs,
scale 1e-4 and three `LOG_DISCOUNT_FACTOR` date axes. EXACT scaled infinity is
.003981926655211021 and L2 .006624713947827545; APPROXIMATE L2 is
.006624713953073177, below eta. Raw 3x3 Jacobian determinants are approximately
.001201014912906. Independent financial derivatives check every raw basis
column at the fitted curves. Both modes together pass 24 direct value and
72 raw derivative gates, with the same refinement and quarter-indicator rules.

Recovery is reported separately from quote tolerance. EXACT parameter errors
versus the frozen target are approximately +1.8273e-7, -4.8169e-6 and
-1.4914189e-4 logDF; APPROXIMATE is nearly the same. Raw quote residuals are
3.6250e-7, 3.9819e-7 and 3.8588e-7. This is a ranked, tolerance-qualified fit,
not evidence of tight node recovery or a tightened solver criterion.

The same original three instruments are prepared for HeldOut, yielding D=30,
55 and 80. `RequireXccyMcIndependent` refuses before draws with
`CalibrationContextMismatch`, `/sampling/mapping`, expected one common mapping
or a separate coordinate allocator. No mixed-D independent basket is claimed.
The local regression preserves this real capability refusal. A union mapping
or explicit disjoint scalar allocator is outside this test-only change and
needs coordinator-owned implementation before that acceptance can close.

### Broader financial and historical derivatives

R2 independently checks seven scenarios across all modes, both spread legs and
both directions: individually flipped rho-df/rho-dX/rho-fX, negative basis,
zero basis, projection=own OIS and nonflat/bucketed curves. These are 84 cases,
336 direct value gates. Six signed/control cases have all nine native axes;
the nonflat MTM foreign-spread pay case has all 30 axes: 84 axes and 336 raw
A/B/Price/par derivative gates pass. All 28 FIXED/no-lag cases agree with
legacy native Price within 1e-10 in common support; lag/reset/MTM native
discrepancies remain distinct. These tests do not claim legacy common-support
native partials for every case.

All six historical/settled cases now have the complete nine-axis native map,
independently differentiated through frozen supplied history: 21 admissible
value gates and 189 derivative gates pass. Missing par is kept absent when A
is zero; passive all-expired outputs have exactly zero derivatives. Supplied
at-V FX FIX=1.17 remains distinct from active payment SPOT=1.10. No global
fixing or product behavior was repaired.

Five new regressions protect selected independent signed/history values and
all 138 associated native axes, the three-maturity quote/axis contract,
noncanonical maturity and mixed-D refusal, D=25/30/40 five-slot coordinate/RNG
traces, and an actual omitted rate-fixing state refused before cashflows.
The three original acceptance tests plus these five pass 8/8 after a fresh
targeted build. The original 96 existing tests retain their earlier XML and
source/dependency identities; they were not re-run as a new full suite.
An initial test-only `Vector_::Append` initializer-list compile error and a
reversed controls CLI invocation were corrected; neither was a product failure.

### Additional frozen MC profiles

Three additional Validation profiles completed x1/x2/x4 with actual pathwise
AAD, 16 batches, seed 1024, Precise and no bridge: rho-df FIXED/foreign/pay
(no lag, D=25), negative-basis RESETTABLE/foreign/receive (D=30), and nonflat
MTM/foreign/pay (D=40). The same original Validation reservation and
16384/32768/65536 paths per batch apply. Direct financial/gradient gates passed
before these new runs. The pre-run freeze records the inputs, profile labels,
executable/archive and analyzer hashes. The later unused-import cleanup and
covariance-shape admission repair are explicitly recorded; neither changes
rules, estimates or financial inputs. No blind-analysis claim is made.

All nine new reports pass Price/par parity and fail precision and combined
acceptance, retaining numerics=false and budget exhaustion. At x4:

| Profile | Price mean | Price 3SE + numeric indicator | Par mean | Par 3SE + numeric indicator |
|---|---:|---:|---:|---:|
| FIXED, flipped rho-df | .2759244776287306 | .023090588414554986 | -.003995518053232769 | .00041744344383781305 |
| RESETTABLE, negative basis | 1.4898619356469027 | .03244383102341592 | .028148217928152135 | .0005890532493701985 |
| MTM, nonflat/bucketed | -3.697739139368145 | .05588455163397504 | .06805259314427302 | .0010058798132338033 |

The frozen limits remain .0025 USD and .000025. All selected spot/basis
gradient parity, precision and combined gates pass at all three prefixes:
2, 2 and 5 native coordinates respectively. Other native parameters and other
statistical combinations are not inferred from this subset. Full paired
covariance and signed denominator confidence pass independent reconstruction.
The strict matrix command exits 1 after preserving all estimates.

Every profile's complete interval/slot list, range endpoints, flags and
consumption are audited. A separate direct MRG32 audit verifies all sixteen
first-normal vectors in every run at their actual large batch offsets, using
full-precision JSON parsing. Its initial default-parser one-ULP discrepancy is
retained as a harness RED; the exact audit passes with no tolerance relaxation
or captured-data change. Profiles and nested prefixes are coupled controls,
never pooled or described as independent basket observations.

### Coverage ledger against section 8

| Requirement | Actual evidence / remaining limit |
|---|---|
| Canonical dates, directions, spread legs, all modes | Original 60 plus R2 84 independent cases; explicit calendar and payment coverage assertions. |
| Flat/nonflat, bucketed, zero vol/a/basis, signed correlations | Independent direct gates and original five kernel/rank controls; full statistical cross-product remains incomplete. |
| Small positive pivot and lag-only mutant | Original normalized kernel reconstruction and isolated 1e-10 lag gate reproduced unchanged. |
| Independent Qf and martingales | Original flat/nonflat coupon and kernel controls reproduced; broader unit/conditional-forward/reciprocal matrix is not newly certified. |
| Historical values, derivatives and par refusal | R2 nine-axis derivatives at all six cases; original refusal controls retained. |
| Snapshot/global/at-V/future histories | Earlier 96-test evidence includes `XccyObservationTest.TestAtValuationPaymentSpotAndSuppliedResetUseDifferentSlots`, `TestAtValuationRatesSuppliedEmptyAndOmittedFreezeDifferentProvenance`, and `XccyMcBoundaryTest.TestFrozenHistoryAndAtValuationResetRemainDistinctFromPaymentSpot`; explicit global mutation between all CRN bumps remains separate work. |
| Source/principal/rebinding authority | Earlier `XccyGaussianExecutionTest.TestPrincipalSwitchesAndImmutableRebindingReachEveryBackend` and observation stale-date/mapping tests; no product edit or second trade authority. |
| Omitted fixing / extra normal / wrong gradient SE | New actual fixing-state omission refusal; exact coordinate/RNG traces at three more D values; original wrong-SE mutant and raw extra-normal trace retained. |
| Three-node synthetic fit and raw axes | Independent quotes, both solve modes, rank and basis partials executed; node-recovery errors reported above. |
| Same-instrument held-out basket | Actual mixed-D refusal before drawing; allocator capability missing, no basket acceptance. |
| Binary64 CRN FD | Original five-row RED remains a production-path qualification gap; 50/80-digit control does not replace it. |
| Public C++/Python/Excel ownership and flags | Await actual DAL-424 merge/acceptance, then execute; no Linux result substitutes for native Windows/XLL/workbook. |
| Performance | Prepare/evaluate times only; no paired baseline or speedup claim. |

## Reproduction and remaining acceptance

Configure/build with the recorded CMake cache and command manifest. The private
driver is compiled separately against the fresh core archive, without a build
file change:

```sh
g++-14 -std=c++17 -O3 -pthread -Idal-cpp -Idal-cpp/test-support \
  -Idal-cpp/externals/rapidjson/include dal-cpp/test-support/xccyacceptancecapture.cpp \
  build/dal425-core/dal-cpp/libdal_cpp.a -o build/dal425-core/xccyacceptancecapture
```

Driver modes are `OUTPUT capture`, `OUTPUT controls`, `OUTPUT crn`,
`OUTPUT mc PHASE FACTOR`, `OUTPUT geometry`, `OUTPUT staged QUOTE_FILE`,
`OUTPUT broad`, `OUTPUT historical`, and `OUTPUT matrix PROFILE FACTOR`.
Matrix profiles are fixed/resettable/nonflat and use Validation only.
The `xccy_acceptance_*.py` tools take the captured
JSONs; their `--help` specifies arguments. The exact executed invocations,
individual exit statuses, raw outputs, identity manifest and freeze history
are supplied with the issue delivery. A strict statistical failure is an
expected acceptance RED, not a passing product test.

Remaining work includes the same-instrument mixed-D held-out basket, the
unclosed coverage-ledger entries and DAL-424's actually merged public/Python/
Excel boundaries. New independent staged/history and correlation/basis-sign
evidence closes those deterministic dimensions, with the recovery limits above.
The base nine-axis long-run result cannot certify every notional mode, spread
leg, direction or nonflat/bucketed statistical combination. No final V1 or
parent acceptance is recommended from this checkpoint.

Preparation/evaluation timings are recorded for the actual new model/script
caller, separately, with no isolated paired baseline. Performance is
**inconclusive**; no speedup, nine-gate pass or formal regression is claimed.
