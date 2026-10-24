# DAL-425 independent XCCY numerical acceptance

## Current decision

Core acceptance checkpoint; **frozen numerical acceptance fails**. This is a
test/tooling PR, with no production repair. Price/par parity passes on the
Validation x1/x2/x4 prefixes, but precision and combined acceptance fail.
Original single-run large-sample native-gradient qualification exists: all nine coordinates
pass parity, eight pass precision/combined, and FX sigma fails both budgets
even at x4. Estimates, actual batches, failures and identities are retained.

This checkpoint is deliberately not final V1 acceptance. DAL-424 is now accepted
on master at `ccec714f`; the incremental M5 boundary results below pass 39
targeted Linux cases, with one additional Windows generated-entry case registered
but not executed locally. DAL-427 is now accepted on master at `7b250160`.
The new prepared-basket continuation below exercises the original staged models;
the legacy unprepared mixed-mapping refusal remains a required rejection.
Frozen precision/combined failures, binary64 FD/eFD qualification, remaining
statistical combinations and absent basket bindings prevent full V1 acceptance. Parent
DAL-396 must remain in progress. No model, financial convention, scale, epsilon,
path reservation or variance-reduction policy was changed to obtain a pass.

## Authority and applicability

Original numerical product base: `4dbdff636bcd92d2a236abe644a3e0c73b9c208a`, actual merged
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
gradients pass parity and precision; all pass combined at x4, with 2, 2 and 5
native coordinates respectively. Nonflat x1 spot par-gradient combined fails:
.002999688863758609 > .0025, despite precision .002428724569543727 passing.
That prefix failure remains in the raw report. Other native parameters and other
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
| Snapshot/global/at-V/future histories | Earlier 96-test evidence includes `XccyObservationTest.TestAtValuationPaymentSpotAndSuppliedResetUseDifferentSlots`, `TestAtValuationRatesSuppliedEmptyAndOmittedFreezeDifferentProvenance`, and `XccyMcBoundaryTest.TestFrozenHistoryAndAtValuationResetRemainDistinctFromPaymentSpot`. The basket continuation adds global mutation between every +/- h/h2/h4 evaluation of the original historical fixture. |
| Source/principal/rebinding authority | Earlier `XccyGaussianExecutionTest.TestPrincipalSwitchesAndImmutableRebindingReachEveryBackend` and observation stale-date/mapping tests; no product edit or second trade authority. |
| Omitted fixing / extra normal / wrong gradient SE | New actual fixing-state omission refusal; exact coordinate/RNG traces at three more D values; original wrong-SE mutant and raw extra-normal trace retained. |
| Three-node synthetic fit and raw axes | Independent quotes, both solve modes, rank and basis partials executed; node-recovery errors reported above. |
| Same-instrument held-out basket | Original unprepared refusal retained. New prepared D30/55/80 execution uses the two original fitted models and frozen prefixes; complete estimates and numerical acceptance are separate, as recorded below. |
| Binary64 CRN FD | Original five-row RED remains a production-path qualification gap; 50/80-digit control does not replace it. |
| Public C++/Python/Excel ownership and flags | Accepted single M5 has 39 independent Linux cases and the include-order repair. Eventual-head native Excel execution remains required. Basket public/Python/Excel bindings are absent and are not counted as passed. |
| Performance | Prepare/evaluate times only; no paired baseline or speedup claim. |

## Reproduction and remaining acceptance

### R3: reviewer admission repairs and M5 dependency check

The consumed M5 wakeup reported an earlier in_review event. On resumption,
PR632 was still open/unmerged; DAL-424 had returned to repair and then delivered
its new Excel head for limited review. No unmerged M5 source is imported and
no public/Python/Excel boundary result is claimed. The original one-shot rule
is already disabled after firing; it is not treated as a continuing merge wait.

Reviewer `59714aee` identified three private-tool P2s: unqualified selected
gradients, an unbound base oracle and truncated covariance/partial axes. R2 had
repaired covariance matrix dimensions, but not all three findings. R3 implements
the remaining checks in the original scope, including the matrix caller, for
limited review.
Six paths are affected: the statistics and matrix tools, a new standalone
Python regression module, private capture metadata, the extended oracle's input
hash field, and this record. Production/build/public documentation remain frozen.

Statistical references now require their actual deterministic capture SHA256
and separately prepared contexts. The `contexts` driver mode prepares the
four fixed reference fixtures without evaluating MC. It checks full economic
source and complete `BindingIdentity` before admitting the base instrument's
different display-ID identity. It exports expected source, binding, Price
measure, complete model contents, mapping/dimension and native/selected axes.
The loader matches these to the hashed reference capture, not the incoming MC
argument. Thus a matching model identity alone cannot conceal changed inputs.

Old base records lack explicit binding/measure fields; old R2 matrix records
also lack instrument identity. Their use is restricted to exact whole-record
digests from the two already-published, manifest-verified evidence archives.
The registry is generated from those immutable archives and checked against
the prepared reference contexts and original MC source continuity. It is not
generated from the capture currently passed to `analyze`. Missing legacy
provenance fails closed. Future private MC captures export instrument, binding
and measure explicitly through metadata wrappers. No old file is rewritten or
represented as having contained the new fields.

Before statistics, each selected A/B/Price/par gradient must be present,
uniquely labelled and directly qualified, with frozen native scale/FD step,
finite nonnegative indicators, separate quarter budgets and combined direct
gate. Values undergo the corresponding check. Covariance must be exactly
`(2+2*nLabels)` square; every batch and aggregate db/da and gradient/SE axis
must have the expected length before any zip or projection is used. The full
covariance comparison and all existing SE formulas/tolerances remain unchanged.

The first 12 focused regression tests reproduced 20 failed subtests and one
missing-reference error on R2. They pass after repair; the final 17-test set
also protects reference hashes, context/model-content mismatches, explicit
binding/measure, distinct display IDs, legacy provenance and full-sized wrong
covariance entries. Commands and RED/GREEN logs are in the R3 evidence:

```sh
python3 -m unittest discover -s dal-cpp/tests/script -p test_xccy_acceptance_statistics.py -v
```

Eleven original real-data mutations are refused. Another 28 RED-to-GREEN
controls cover both entry points and every R2 matrix profile, including extra
da and removal of the last expected label (the base FX-sigma failure cannot
silently disappear). The original five reviewer
bypass examples were re-executed against the exact `59714aee` helper source,
then rejected by the repaired helper. Both original and R2 nine-report sets
compare exactly, including every precision/combined failure and prefix RED.
Non-strict analysis succeeds; both strict statistical commands still exit 1
after retaining reports. The broad independent oracle was rerun for its new
capture-hash metadata; its values/derivatives/refinements are unchanged.
CCN remains at most 8 and Ruff F/C901 passes. No long MC, core/full suite or
performance matrix is rerun for these admission-only changes. The valid R2
8/8 C++ and earlier 96-test identities remain distinct prior execution.

The original `Mc`, `MatrixCapture`, `MatrixFixture` and `MatrixLabels` bodies
remain byte-identical to R2. The new executable is used for metadata only;
its identity is separate from both long-run executables. The core archive
remains `cd610b9926e54f234642e758cba12963fd0b7d7a2a8b3d46ace2001b834c3ee7`.

The CRN gate map is also explicit. On original 4x8 data, 27/27 bare paired
expectation-parity diagnostics pass, while five of nine binary64 fixed-path
hard-floor rows remain RED. Observed raw mean h/h2/h4 movement gives at most
`1.3145040611561853e-7` under the descriptive four-times refinement indicator,
below the frozen Price eFD cap. This alone does not bound complete binary64
roundoff/eFD, which remains unqualified. The 50/80-digit fourth-order control
and raw binary64 central FD are separate; neither paired parity nor the wide
control replaces the original hard-floor RED. No new FD samples or h changes
were used.

Original `budget_exhausted` flags are retained for exact report compatibility.
For x1/x2 they mean the current prefix failed; only x4 consumes the full frozen
phase reservation. No budget/VR/scale/model decision has been changed.

Statistics CLI calls now additionally require `--reference-capture FILE` and
`--contexts FILE`; old records also require `--legacy-records FILE`. These
are qualification inputs. The legacy-registry generator and complete actual
invocations accompany the evidence. Unqualified old CLI invocations are
refused rather than silently selecting a base reference.

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
`OUTPUT broad`, `OUTPUT historical`, `OUTPUT contexts`, and
`OUTPUT matrix PROFILE FACTOR`.
Matrix profiles are fixed/resettable/nonflat and use Validation only.
The `xccy_acceptance_*.py` tools take the captured
JSONs; their `--help` specifies arguments. The exact executed invocations,
individual exit statuses, raw outputs, identity manifest and freeze history
are supplied with the issue delivery. A strict statistical failure is an
expected acceptance RED, not a passing product test.

At R3, remaining work included the same-instrument mixed-D held-out basket, the
unclosed coverage-ledger entries and DAL-424's actually merged public/Python/
Excel boundaries. New independent staged/history and correlation/basis-sign
evidence closes those deterministic dimensions, with the recovery limits above.
The base nine-axis long-run result cannot certify every notional mode, spread
leg, direction or nonflat/bucketed statistical combination. No final V1 or
parent acceptance is recommended from this checkpoint.

Preparation/evaluation timings are recorded for the actual new model/script
caller, separately, with no isolated paired baseline. Performance is
**inconclusive**; no speedup, nine-gate pass or formal regression is claimed.

## M5 independent caller checkpoint

The original branch and PR631 continue. Accepted master `ccec714f63befee9202fd5b6b324999068e88c1d`
was normally merged at `43a2fd6077fa293ee698bbc477f321d1c50d49d8`, without
rebasing, importing PR637, or changing a product/build/generator file.
The two upstream commits are accepted M5 and the accepted DAL-426 design.
GitHub and Multica's direct M5 link confirm the actual merge. PR637 was still
unmerged at the dependency check; its successful basket terminal remains outside
this checkpoint. Current issue reads show this run as the sole active writer.

The pre-execution fixture/gap/semantics record and subsequent harness notes are
in the delivery's `freeze.md`. New runs replay technical prefixes of already
observed reservations; they are not fresh independent statistical evidence and
do not reset HeldOut. Every representative uses N110, the original dated source,
4 batches x 8 paths, seed1024, Precise normals, no bridge/VR, and an explicit
empty snapshot. Ordered labels are FX sigma followed by FX spot.

| Representative | Measure | Phase | First path | Caller checks |
|---|---|---|---:|---|
| FIXED | Price | Fit | 0 | Public, Python, Excel against direct core |
| RESETTABLE | BasisSpread | Validation | 262144 | Public, Python, Excel against direct core |
| MARK_TO_MARKET | Price | HeldOut | 1310720 | Public, Python, Excel against direct core |

Complete serialized coefficients, 6x6 covariance, two raw partials and their SE,
normal coordinates, mapping/range identities and flags match core. Public
top-level mean/SE and Python getters also agree; phase intervals end after 32
paths and consumed coordinates equal 32D. Public GaussianFast matches direct
core moments on the same prepared source. These checks establish caller parity,
not independent financial/precision acceptance.

An intermediate audit found that the reused M5 Python fixture's default spread
leg differed from the explicit C++ fixture, with different model archive names.
Those earlier per-language results are retained as `precanonical-*`; they
cannot establish identical-input parity. Four small immutable private archives
now supply the exact source/full binding and model to all three representative
callers. C++ verifies them against the unchanged native fixture/model. No second
native product is manually synchronized. The offline comparison helper checks
all three complete serialized payloads **byte for byte across all three
interfaces**, and passes 3/3 phases. Final XMLs are `final-public.xml`,
`final-excel.xml` and `new-python.xml`.

| Technical case | Mean | SE |
|---|---:|---:|
| FIXED Price / Fit | 0.25536469142407153 | 1.025376786246393 |
| RESETTABLE BasisSpread / Validation | -0.0663751238480813 | 0.03328067763458021 |
| MTM Price / HeldOut | -3.79312645749916 | 5.084124755885658 |

These small-prefix estimates have no precision acceptance claim. Complete raw
gradients, gradient SE and covariance are retained in the XML and
`cross-interface.json`. Missing phase, duplicated phase, failed caller case and
a single corrupted partial-SE field are all refused by the comparison helper; original and mutated
XMLs plus exact refusals are retained.

Successful explicit staged evaluation feeds the resulting immutable Gaussian
model into core/public MC and Python MC after input destruction. The public
three-node staged spread differs from its report by about 5.13e-16, inside the
predeclared 1e-12 spread gate. Excel additionally exercises its actual staged
failure entry with max_evaluations=1: the last complete validated Gaussian model
is distinct from the retained curve, survives spec/outcome destruction, and
feeds BasisSpread MC with complete core parity. This is not fit convergence.

Python's actual native entry barrier permits another thread to mutate the
original Python label list after entry. The result preserves the copied labels.
The held-GIL negative handshake times out as expected in the freshly run M5
test. Results, reports, exceptions and partial getters survive external input
destruction; returned lists cannot mutate the owning result. Unsupported and
duplicate labels retain `/mc/parameter_labels`; invalid smoothing retains
`/simulation`. Nested bool/text/null/nonfinite values and both int32 overflow
endpoints are refused.

Excel typed outcomes retain complete payloads and the exact ordered
`parameter_label/native_partial/standard_error` spill after inputs are destroyed.
The precision factory reads fields after blank rows, refuses bool/text/nonfinite
values at original row4/column2, and refuses unknown/normalized duplicate keys at
row4/column1. The new lifetime case creates no repository entries. A separate
Windows-only test calls the actual generated precision export with trailing
raw error/bool cells after two blank rows and asserts function/field/original
location. It is registered through the existing test glob; Linux does not
compile or execute its Windows body.

### Execution and repair accounting

- New tests: **2/2 public + 3/3 portable Excel + 15/15 Python pass**.
- Existing M5 tests freshly run: **3/3 public + 7/7 portable Excel + 9/9 Python
  pass**; the 18-case Python value-only matrix is explicitly deselected.
  These cover required phase/nested inputs, absent/null/empty/future snapshots,
  capacity refusal, Validation-only eligibility, retained insufficient-budget
  estimates, absent gradients and staged failure ownership.
- CTest discovery lists the five new Linux C++ cases. One additional native
  Windows case has not executed locally. No full-suite run is claimed.
- Harness repairs only: private member access changed to its getter; a
  keyword-only Python argument and canonical input's instrument-ID assertion
  corrected; the initial unintended bitwise staged
  scalar assertion changed to the already frozen 1e-12 spread gate. Initial
  compiler/test logs remain in the evidence. Interim failed-harness binary
  hashes were not captured, so they do not establish a product RED.
- Final Ruff, formatting/diff checks pass; the caller tests/bridge/comparison
  helper Lizard reports 31 functions, maximum CCN7. No product behavior was repaired.

The delivery includes exact commands/exit statuses, XML with raw payloads,
initial failures, incremental patch and source/dependency/configuration/binary
identities. Local configuration remains GCC14.3/C++17/Release/O3, AADET,
built-in matrix kernel, native arch off; Python3.13.9 with pybind11 3.0.4 from
the selected Anaconda environment. An initial attempt to select system
Python3.14 lacked Development.Module and did not configure; the final configure
uses the complete Python3.13 environment.

Core source has no upstream diff since R3. Its archive remains exactly
`cd610b9926e54f234642e758cba12963fd0b7d7a2a8b3d46ace2001b834c3ee7`.
Thus original oracle/long-run evidence retains its recorded scope and failures.
New public/Excel runners and Python extension have separate hashes in the
manifest; old binaries are not attributed to new callers. Accepted M5 native
CI remains historical evidence for unchanged production and original cases,
not execution of the new native test. No workbook was run. No example/build
product change calls for repeating component installation closures.

Performance remains **inconclusive** without an isolated paired baseline.
Focused review of the incremental test/helper/fixture/record delta and
exact-head CI remain pending. PR631 stays draft and DAL425/parent396 stay
in_progress. Successful mixed-D basket, remaining financial/statistical coverage,
FX-sigma precision/combined, five binary64 FD RED rows and full binary64 eFD
qualification remain open. All eighteen original reports, budgets, thresholds
and observed HeldOut identities are unchanged.

### M5 include-order R1

Focused review of `943ad20c5bc0a9353575aba261a2532c11537ac9` found one P1:
Windows.h's `VOID` macro prevents the transitive DAL StackInfoType enum from
parsing in the new raw test. Its include order now follows the existing
`test_script_raw.cpp`: DAL headers precede Windows.h, with the same local
rationale. All helpers, initialization, raw-error/bool branches, test body and
assertions remain byte-identical. The only other tracked edit is this record.

Fresh GCC14.3/C++17 `-fsyntax-only` controls extract the actual test header
order and introduce `#define VOID void` at the first Windows-bearing include
(Windows.h or `_xlcall.hpp`, which itself includes Windows.h on Windows).
Original order exits **1** at the actual generated enum; repaired order exits
**0**. The reviewer's unchanged minimal RED/GREEN sources also freshly exit
**1/0**. Exact argument vectors, source hashes and full diagnostics accompany
the R1 delivery. The repaired raw source SHA256 is
`9fd8b658e3d4949394855bd551e388362bdcc15a62fb0ee06db7cc1510aab533`;
the unchanged body after `using namespace Dal;` hashes to
`9a63464034b1f0f4c6a1afd7165873eeb556523be5df1bb73b1c8ee27e9c0370`.

This is a Linux macro/header compilation control. No local MSVC toolchain,
native XLL or workbook execution is claimed. A fresh Windows `dal_excel_tests`
build and actual execution of
`ExcelXccyAcceptanceRawTest.TestGeneratedEntryChecksEntireSelectedRange`
remain required for this P1's native platform acceptance; old M5 CI cannot
substitute. The original 39 Linux cases and numerical reports are retained
execution evidence, not R1 reruns. Source/dependency/configuration/binary hashes
are checked separately in the R1 evidence; the core archive identity above and
all original evidence files are preserved. Formatting and diff checks pass.

No production, build, generator, fixture, threshold, budget or numerical
algorithm changes are included. Prior review conclusions outside this P1 retain
their stated scope; limited R1 review and new-head CI remain pending. PR631
stays draft, DAL425/396 stay in_progress, and all financial/statistical gaps,
precision/combined failures, binary64 FD RED rows and old HeldOut identities
remain unchanged. No new statistical samples or performance claim are added.

## Prepared original basket: independent continuation

DAL-427/PR637 was actually accepted and merged as
`7b2501603d69d4fa3ba008be1d026515ed9fe32a`. This original branch normally
merged that accepted master in `7940d0450f551091f90e36196900899d31667b85`,
whose parents are `2ed0581c2cb5ab6f5784f8db2bcaf2a8d82d352c` and
`7b2501603d69d4fa3ba008be1d026515ed9fe32a`. There was no conflict, rebase,
force push or import of an unmerged sibling. The M5 raw-test include repair
and body are byte-identical to `2ed0581c`; its source-only limited Approve
is retained, while its eventual-head native platform condition remains open.

This continuation changes four independent core test/helper files and this
record. No product, public API/binding, build, RNG, solver, fixture, financial
convention, public guide or CHANGELOG is authored here. The accepted DAL-426
and DAL-410 control hashes remain respectively
`3c5ae6eed21ed0582ef0ba70811f4692efbcead2b69a184bbe5e0b5081a140ad` and
`c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`.

### Freeze, reuse and actual callers

Before new samples, both original EXACT and APPROXIMATE fits were reproduced
and matched the retained R2 parameters **exactly**. Source/instrument identities,
independent calendars/cashflows, full native labels, original direct values,
full direct derivative arrays, dated grids and mapping hashes matched R2.
Canonical maturities/payments/basis knots and the approximately
`-1.4914e-4` third-node recovery error remain as originally reported. The
invalid `2028-11-06` maturity and legacy mixed-map refusal remain real REDs.

The private caller is `dal-cpp/test-support/xccyacceptancebasketcapture.cpp`.
Its frozen source SHA256 is
`60074a8b69a91ceaec30802e924e3c02fcd8daa7757401914c7047a5904cef8a`,
actual capture executable SHA256
`b4151ec3502b0e3426a77b74e3c2540094fb17430f95b53022cba22b0ade6aed`,
and rebuilt core archive SHA256
`fffcc05c81a462d06f5bf1a04af9ca8739f2e0628baea80dcde2b49e3b5e6559`.
The pre-run manifest contains 3,881 source/dependency/configuration/artifact
identities. The existing Release/GCC14.3/C++17/O3/AADET configuration and
explicit supplied empty fixing snapshot are used. Compiled execution is the
captured header's `DEFAULT_COMPILED=true`; the generator/precision/seed are
MRG32 `/1`, Precise, 1024. The capture/source/binary/archive manifest was frozen
at **2026-10-23T23:55:54Z**, before these requests started; the executed capture
binary and archive were not changed while running. Original executables, raw
eighteen reports, oracle inputs and previous manifests remain separate evidence.

The separate `analysis-freeze.json` is timestamped **00:06:44 UTC** and retains
its original declaration `statistical_long_outputs_observed=false`. The two x1
requests had already completed at **00:06:38/42**. That declaration records the
claimed observation state at that time, not an assertion that no long output
existed. The x2 requests completed at **00:18:38/42**. Subsequent admission and
defined-stream/ideal-quantile diagnostic revisions followed observation of
outputs, including the x2 tail RED below. `final-analysis-freeze.json` records
the final analyzer/test and extraction identities at **00:23:02 UTC**, after
those observations. Those final identities are post-observation tooling, not
pre-observation freezes. Both original metadata files and the pre-run manifest
remain unchanged; neither samples nor their observation status are relabeled.

The six requests are both solve modes at HeldOut B=16, n=16384 times 1/2/4.
The seven requested axes are spot, the three fitted basis nodes, USD sigma,
EUR sigma and FX sigma, in native-map order. Complete rows contain all requested
derivatives; the full native chain is exported separately. The four other
native curve labels are not advertised as newly sampled basket columns.
All seven direct A/B/Price/par derivative references are qualified: 72 retained
basis gates and **96 new spot/sigma gates**, all passing. The original 24 direct
value gates are retained. New derivatives use the unchanged independent dated
cashflow/Brownian integrals, three precision refinements, 50/80-digit diagnostic
controls and h/h2/h4 scales. Wide arithmetic does not close binary64 eFD.

The exact scalar blocks are `[0,70778880)`, `[70778880,200540160)` and
`[200540160,389283840)`, T=2359296. All nine full-phase reservations, every
selected batch's local path/absolute scalar endpoints and actual consumption
are independently checked. The middle offset modulo 55 is 40; it is never
rounded to a path index. The source-use test records **114 observation uses /
570 five-slot joins** for both fitted models, with event/statement/node/source
row, original/canonical index, fixing time, model slot and absolute coordinate.
Every recorded batch-start normal is checked using independent Python bigint
MRG32 recurrence/skip arithmetic and an independent inverse-normal routine.
The first member's traces also exactly match the earlier single HeldOut traces.
Those coordinates were already observed: this family is not untouched holdout.

Accepted DAL-427 RNG/helper/lifetime/partial-failure evidence is reused within
its exact identities and accepted R1/integration changes. The new evidence
contains the path-by-path identity comparison: scalar adapter, range and
statistics implementations are unchanged from the independently audited version;
source-row/include changes retain their later limited approvals. No 23,760-normal,
24-single-case, old R3, 39-case M5 or eighteen-report matrix is rerun. The old
unprepared refusal alone is freshly checked with its narrow test filter.

### New tests, admission controls and numerical interpretation

Fresh registered C++ execution is **5/5**: four new tests plus the original
noncanonical-maturity/unprepared-mapping refusal. New tests cover the fitted
source trace, full par/raw-gradient Jacobian and projected covariance, genuine
zero-sigma derivative unavailability with owned zero-draw/no-estimate payload,
and historical global mutation between every +/- h/h2/h4 call. The history
test initially supplied an omitted snapshot, correctly receiving
`InvalidCalibrationPlan /context/fixings`. It now retains that rejection and
uses the explicit original historical values required by the basket contract.
Initial standalone include-path and enum-type compile errors were harness
repairs only; their diagnostics are retained. No product repair was needed.

New Python admission regressions pass **4/4** (with six malformed-data subcases).
The actual capture audit additionally rejects **16** covariance/axis/summary,
phase/offset/extra-normal, source/full-binding/model and unqualified-reference
mutants. Three draft-tool admission bypasses were reproduced against the saved
draft, then rejected by the final helper. Labels, units, all requested summaries,
every square matrix dimension, frozen source/context/model and direct derivative
scale/h qualification are checked before numerical conclusions. Ruff F/C901
with maximum complexity 8 and C++ formatting pass.

A further Lizard check found ten over-complex draft functions (maximum CCN26).
Pure helper extraction reduces every new C++/Python function to CCN<=8. Saved
pre-refactor sources and function/patch identities are retained. The executed
long-run source above remains archived as `capture-executed.cpp`; its binary
is never overwritten. The separately rebuilt final driver has byte-identical
complete small control payloads (excluding timing), with the same core archive,
model/input/mapping/labels and evaluation calls. Financial formulas, every
existing report field and statistical flags compare identically before/after
extraction. No long samples are repeated for this code-quality change.
The final private driver source SHA256 is
`c724cbba5fdb32cd833e4495728e1278644a29b0bafd009e113c296b241960dd`;
its separately named rebuilt executable SHA256 is
`65d92b31708d100ce577ebca91393584897d7c9fad154acf8b48c3ebcf949474`.
It is used only for the small payload-equivalence controls, not relabeled as
the executable that generated the six long captures.

An initial x2 trace check against an ideal Gaussian quantile exposed a separate
legacy transform accuracy limit. At absolute scalar coordinate **339476552**,
independent bigint MRG32 gives u=`0.9999880110839163`; the defined `/1` Precise
transform returns `4.224211942570607`. An independent 85-digit inverse gives
`4.224211941966543988289141378120059465...`, discrepancy about `6.04063e-10`,
failing the new ideal-quantile 5e-13 diagnostic. Neighboring coordinate controls
and the accepted independent scalar oracle confirm the actual stream position
exactly. `dal-cpp/dal/math/inversecdfscalar.cpp:58` caps the tail polishing
exponent at 8; this is unchanged legacy RNG behavior, not an allocator offset.

The initial trace RED, raw scalar probe, high-precision values and neighboring
controls are retained. Coordinate auditing now explicitly checks the defined
versioned transform, while **preserving ideal-quantile failures as RED** in a
separate diagnostic. Neither tolerance was relaxed. This does not establish a
global financial bias bound for the approximate normal transform; no RNG/model,
financial threshold or numerical verdict is changed. The new regression keeps
defined-stream agreement distinct from ideal Gaussian accuracy. Any product
repair belongs to the original owner/coordinator, outside this test-only scope.

For every complete result, independently concatenate the B rows in original
member order. Reconstruct all **48x48 C_batch entries**, C_mean=C_batch/16,
all diagonal member blocks and all measured cross-member blocks. Reconstruct
the complete Price/raw-gradient Jacobian and its **24x24 output covariance**;
derive the full joint par/raw-gradient Jacobian and covariance from the same
rows. The actual core par interface is separately tested on both fitted models.
There are 16 paired batch vectors, not 48 independent observations. Rank is
at most 15; no inverse, economic portfolio covariance or exact-zero cross-block
condition is imposed. Signed annuity confidence and delta-method ratio/gradient
SE use all B/A/dB/dA cross terms. Same-factor model differences use paired rows;
prefixes are overlapping regrouped samples and are never pooled.

The two small 2x3 diagnostic captures give 54 selected Price and 54 par CRN
comparisons, retaining paired FD-minus-AAD differences and their SE at h/h2/h4.
Draws and absolute endpoints match for both signs and all bumps. These small
calls are technical controls, not additional precision samples or a binary64
roundoff certificate. All original five FD RED rows and full eFD qualification
gaps remain, even where these controls or high-precision direct gates pass.

Exact commands and exit codes, raw six result captures, complete gate flags,
covariances/projections, frozen identities, XML source trace, draft bypasses,
initial harness failures, ledger and applicability comparisons accompany this
issue's basket evidence attachment. Timings come from concurrent foreground
calls without an isolated paired baseline: performance remains inconclusive.

### Coverage and retained gaps after the basket continuation

| Area | Evidence / limit |
|---|---|
| Original staged instruments and models | Exact R2 parameters, sources, dated cashflows, mappings and full native map context; same two immutable fitted models, no HeldOut refit. |
| Direct financial qualification | 24 value and 168 selected derivative A/B/Price/par gates; 96 spot/sigma gates are new, prior basis gates are reused. |
| Original basket execution | Six frozen HeldOut requests, 16 complete paired rows per request, seven requested native columns per member; financial verdict is separate from completion. |
| Scalar/phase provenance | All nine reservations and all actual batch endpoints; independent bigint MRG32 trace, source-use/slot joins and previously observed first-member coordinates. No pooling or fresh-holdout claim. |
| Full statistics | Complete C_batch/C_mean, cross blocks, Price/par/raw-gradient Jacobians, denominator qualification and paired SE; rank bound is reported without inversion. |
| History and derivative boundary gaps | New inter-bump global-mutation case and genuine zero-sigma owned unavailability; prior accepted owning partial, tape, candidate, arithmetic/range and phase controls retain their identity-scoped applicability. |
| Frozen statistical acceptance | Parity, precision and combined flags are separate for every member, measure, axis and prefix; raw estimates and all failures remain available. |
| Legacy Gaussian transform | Defined stream agrees; observed ideal-quantile diagnostic RED and unqualified global financial bias bound are retained. No RNG repair is included. |
| Production binary64 FD/eFD | Original five RED rows/full qualification gaps persist; small CRN and 50/80-digit controls do not close them. |
| Remaining financial/statistical matrix | Earlier signed/lag/history/mode/zero/rank cases retain their documented scope; no new exhaustive cross-product or broader Qf/conditional-forward/reciprocal certificate is claimed. |
| Three caller boundaries | Original single M5 39-case Linux evidence remains scoped to its actual source/config/binaries. New eventual-head native Excel raw-case execution remains necessary. |
| Basket bindings | Absent at the tested core head; DAL-429's original implementation chain is already active. This tester neither implements nor marks those boundaries passed. |
| Published guide / performance | DAL-428's accepted public guide/CHANGELOG window is separate; neither is edited here. No isolated paired performance gate, hence inconclusive. |

DAL-425 and parent DAL-396 remain in_progress. This is independent execution and
failure-preserving evidence, not approval of complete V1 or calibrated quote risk.

### Actual six-run result and frozen-budget verdict

All six foreground executions exited **0**. x1 took about 644 seconds per mode,
x2 about 1,362 seconds and x4 about 2,833--2,835 seconds. Each completed all
three members and all 16 finite 48-column coefficient rows. Actual scalar
consumption per mode is 43,253,760 / 86,507,520 / 173,015,040 for x1/x2/x4.
The total generated count across repeated modes/prefixes is 605,552,640;
overlap is explicitly retained, not interpreted as a larger independent budget.
The largest prefix ends exactly at each HeldOut reservation end, including
389,283,840 for D80. All **15,840** recorded batch-start normal coordinates
match the defined stream. The ideal-quantile diagnostic RED above remains in
both x2 captures; not observing that point at another batch start erases nothing.

| Solve mode | Prefix | Complete estimate | Parity | Precision | Combined | Full reservation exhausted |
|---|---:|---|---|---|---|---|
| EXACT | x1 | yes | pass | FAIL | FAIL | no |
| APPROXIMATE | x1 | yes | pass | FAIL | FAIL | no |
| EXACT | x2 | yes | pass | FAIL | FAIL | no |
| APPROXIMATE | x2 | yes | pass | FAIL | FAIL | no |
| EXACT | x4 | yes | pass | FAIL | FAIL | yes |
| APPROXIMATE | x4 | yes | pass | FAIL | FAIL | yes |

There are 288 separate member/measure/selected-gradient statistical gates:
288 parity, 216 precision and 214 combined pass. The complete report retains
the other gate failures explicitly; no aggregate hides a selected axis.
Every Price/par precision gate fails at every prefix. FX-sigma Price/par
gradient precision also fails for every member/prefix in both modes. The
minimum signed annuity confidence is 54.67885818283065, so unavailable
denominators do not explain or mask these results.

The maximum-prefix estimates are:

| Mode | D | Price mean (USD) | Price 3SE | Par mean | Par 3SE |
|---|---:|---:|---:|---:|---:|
| EXACT | 30 | -3.690116360669866 | .0582977986057430 | -.06646643739571238 | .001056699799877131 |
| EXACT | 55 | -6.021485844381295 | .04630961138092355 | -.05440261628807351 | .000419843718790972 |
| EXACT | 80 | -8.269656535302552 | .04567276262235123 | -.04979993334343368 | .000271021442846936 |
| APPROXIMATE | 30 | -3.690116360669856 | .05829779860574308 | -.06646643739571222 | .001056699799877132 |
| APPROXIMATE | 55 | -6.021485844381334 | .04630961138092261 | -.05440261628807405 | .000419843718790966 |
| APPROXIMATE | 80 | -8.269656535302042 | .04567276262234753 | -.04979993334343384 | .000271021442846913 |

These Price 3SE values exceed the .0025 USD precision budget; par 3SE exceeds
.000025. At EXACT x4 the FX-sigma Price 3SE values are approximately
.497871/.383331/.427202 against .25; par values are .00902832/.00347837/.00254231
against .0025. The raw gradients, references, indicators and all paired SE
remain in the report. The two extra combined failures beyond precision are
also retained rather than converted into a broader gradient pass: both modes'
D80 x1 EUR-rate-sigma Price combined indicator is about 2.83181753 against 2.5.

Non-strict complete analysis exits **0**; `--strict` writes the byte-identical
complete report and then exits **1**. All six financial reports and paired
model statistics compare exactly with the saved pre-extraction analyzer. No
old eighteen-report sample or new prefix was added to improve this verdict.
The two x4 reservation-exhausted results remain numerics-failed; no complete
V1, quote-risk or performance acceptance is recommended.

### Focused basket admission repair and accepted-master integration

The original reviewer requested three P2 repairs at fixed head
`d3a388928ebdc1817db558704a58b3f826a35a08`: incomplete oracle member contexts,
truncated direct derivatives/unread native chains, and the analysis timeline
claim corrected above. This checkpoint changes only the basket Python helper,
its Python regression module and this record. The normal accepted-master merge
is `ecb148241a9d524be726c4e2ddc2927bb58259aa`, with parents
`d3a388928ebdc1817db558704a58b3f826a35a08` and
`f5cf78a638806ff682772603f0d744399032bb33`. It has no conflicts and inherits
only accepted PR638's guide/CHANGELOG changes beyond the already integrated
core. It imports no unmerged DAL-429 bindings. Complete incremental patches,
new source/head identities and merge audit accompany the repair delivery.

The helper now independently requires all seven member-context fields
`id/source/binding/context/mapping/instrument/measure`, then compares that fixed
set. Before any derivative iteration, both direct and retained A/B/Price/par
matrices must have exactly four finite full-width rows. The exported native
chain must have the complete retained moment-input row count and native-label
width, finite entries and exact coordinate correspondence. Its expected blocks
are reconstructed from the original model's curve snapshot Jacobians and
ordered spot/curve/sigma axes, including the curve valuation-row exclusion;
the current full matrices are 4x11 and 11x11. This adds evidence admission, not
a new economic mapping or financial tolerance.

Existing Python tests first passed **4/4**. Four focused tests were added before
the repair; the final module fails against saved d3 and passes **8/8** after
repair. Its negative subcases cover all seven missing reference fields with
synchronously wrong prepared/executed fields, matrix dimensions/finiteness and
wrong native row/column coordinates; the valid control includes a nonidentity
curve Jacobian. Separately, **21** real EXACT-x1 malformed-input probes are now
refused. Thirteen had been admitted at d3, including all four original reviewer
probes: missing reference binding plus wrong full binding, empty derivatives,
missing par derivative row, and empty chain. The other eight were already
refused and remain refused. All **16** earlier real-capture negative controls
remain refused. Diagnostics and per-case d3/repaired outcomes are retained.

Offline replay of the six original EXACT/APPROXIMATE HeldOut x1/x2/x4 captures
admits the complete data and produces the **byte-identical entire report**,
including audits, all financial fields and paired model comparisons. There is
no schema or field diff. Its SHA256 remains
`cca7aac898607d6b2eef7f5d477e259cbff7fd1f167399cb55da35bddbc51327`.
Non-strict exits **0**; strict saves that identical complete report and exits
**1**. The 288/216/214 parity/precision/combined pass counts and all six overall
FAILs remain. The changed Python modules contain 59 functions, all CCN<=8;
Ruff F/C901, Markdown and diff checks pass. No C++ rebuild, long capture,
unchanged RNG probe, old eighteen-report/39-case matrix or broader performance
matrix is repeated for these admission/record-only changes.

Original capture and final extracted binary identities above remain distinct
and unchanged. The ideal Gaussian tail RED, `financial_bias_bound_qualified=false`,
all original precision/combined/FX-sigma and five binary64 FD REDs, full eFD and
remaining coverage gaps, exhausted x4 reservations, inputs/budgets/scales/
thresholds and already-observed HeldOut status remain. The original reviewer's
changed-head helper/record/integration review and eventual-head Windows
`dal_excel_tests` build plus actual selected-range raw-case execution remain
necessary. DAL-429 boundaries require their separate serialized assignment
after accepted merge. DAL-425/396 stay in_progress; this checkpoint grants no
complete V1, Windows workbook, quote-risk or performance acceptance.

## Accepted basket bindings: independent caller checkpoint (2026-10-24)

PR639 actually merged at 02:16:03 UTC as accepted master
`a5da3ecf37bf1dcbfa81097449ba04a63627139f`. GitHub, the direct DAL429 link
and fetched master agree. Normal merge `ea179509f20e80bd755b825cc5fa4bcdb08dd03b`
has parents `8daa65134bc7765dc8abb2f4d34fb2be29ef68f4` and that master, without
conflicts. Its 37 inherited paths are accepted PR639, including product/generated/
build files; they are not new tester edits. No core source changes were inherited.
No reset, rebase, new tree/PR, force push or evidence cleanup was used. This run
authors only independent tests, private fixtures/helpers and this record.

### Inputs, scope and observation identity

The technical input declaration in `acceptance-bindings/freeze.md` has original
mtime **02:23:36.905207 UTC**, before new caller observations. It specifies the
existing immutable acceptance model archive, explicit empty fixing snapshot,
three dated MARK_TO_MARKET sources (2027-11-04, 2028-05-04, 2028-11-04), Price/
BasisSpread/Price and byte-sensitive member IDs `A`, `a`, and an escaped quote/
newline tail ID. The new basket fixture archives are exported from the accepted
core test factory, then loaded unchanged by all callers. They are technical
fixtures, not replacements for the original fitted financial profiles.

All callers use compiled execution, Precise stream1, seed1024, no Brownian bridge
or smoothing, **3 paths x4 batches**, and ordered native labels FX sigma then
FX spot. Fit/Validation/HeldOut are explicit technical phase checks. They do not
reclassify observed financial samples. The one 5-batch technical prefix control
checks overlap rejection; it does not extend any original financial reservation.
No original financial capture was rerun or pooled.

The final helpers/tests were revised after initial observations. They are not
claimed to have been frozen before observation. Initial authoring failures are
retained: missing standalone exporter include path; missing raw-test closing
guard; Python simulation constructor defaults did not explicitly supply the
declared settings; preflight may retain a computed native-map identity before
candidate geometry qualification; old report schema lives in `summary.schema`.
These were test setup/assertion corrections, not product defects or product REDs.
The final tools have a separate timestamped identity, and pre/final complete
caller payloads remain byte-identical after the final helper extraction.

### Existing tests, new tests and negative controls

- Existing accepted basket tests were rebuilt and run independently: public
  **6/6**, portable Excel **6/6**, Python **14/14**, exit0. The Python cases
  include actual Prepare/Evaluate GIL handshakes and the held-GIL negative
  timeout, required inputs, early/late errors and candidate geometry controls.
- New tests pass: public **2/2**, portable Excel **3/3**, Python **15/15**, exit0.
  All three phases have complete result JSON byte-identical across public C++,
  Python and portable Excel, each also compared to a directly executed core
  result on the same archives. This covers the full 4x18 coefficient rows,
  18x18 batch and mean covariance, 9x18 output Jacobian, 9x9 output covariance,
  axes/units/order/ID bytes, selected partials, source/binding/context/model/
  report identity and actual coordinates. Python independently reconstructs all
  covariance cells and the output projection from the captured rows; this is
  a technical statistics check, not a new financial precision qualification.
- Owned member/allocation/report getters survive deletion and GC. New native
  entry handshake mutates the Python member list and label list after native
  copy. Late-member/late-batch/last-member-preflight/early errors retain their
  correct partial/report/candidate alternatives after all input/error owners
  are destroyed. Last-member preflight consumes zero coordinates and exposes
  no candidate. An actual late final-member annuity-floor failure through the
  Excel facade retains its true candidate and two complete members, with four
  coefficient batches but no fabricated final estimate for the failing member.
- New Excel checks distinguish Prepared/Estimate/Failed and unavailable getters,
  the outcome/result/partial JSON families and old report1. Nested wrong types,
  bool/int32 overflow, trailing unknown and normalized duplicate settings refuse.
- The new Windows raw request case and the existing mandatory selected-range
  case run **2/2** through actual generated includes, source-extracted real raw
  converters/validators, and freshly rebuilt real Excel factory/runtime objects
  using the inherited Linux storage/return ABI adapter. The new case checks a
  raw error beyond blank rows with original row4/column2, typed sampling/fixing
  handles, all required omissions, scalar phase and table dimension limits.
  A private copy disabling complete-range validation/truncating conversion makes
  **both tests fail**, exit1; intact source passes, exit0. No production mutation.
- **10/10** mutations of actual caller XML refuse: cross-member/mean/output
  covariance, output Jacobian, axis unit, member ID bytes, incomplete status,
  missing/duplicate phase and failed caller case. Complete positive XML admits.
  The new tracked helpers/tests contain **35 functions, maximum CCN8**; Ruff,
  clang-format, Markdown and diff checks pass. CTest discovers five portable
  C++ cases; Python uses its existing test registration. No build edits needed.

Primary commands (repository root; complete logs/XML and exact command/exit
records are in the attached `acceptance-bindings` evidence):

```bash
cmake --build build/dal425-core --target dal_public_tests dal_excel_portable_tests _dal -j4
build/dal425-core/dal-public/dal_public_tests --gtest_filter=PublicXccyMonteCarloBasketTest.*
build/dal425-core/dal-excel/dal_excel_portable_tests --gtest_filter=ExcelXccyMonteCarloBasketTest.*
python -S dal-python/tests/run_build_pytest.py build/dal425-core/dal-python /home/wegamekinglc/anaconda3/lib/python3.13/site-packages dal-python/tests/test_xccy_monte_carlo_basket.py -q
python -S dal-python/tests/run_build_pytest.py build/dal425-core/dal-python /home/wegamekinglc/anaconda3/lib/python3.13/site-packages dal-python/tests/test_xccy_acceptance_basket.py -q -o junit_family=legacy --junitxml=acceptance-bindings/new-python.xml
python3 acceptance-bindings/run-final.py
python3 acceptance-bindings/generated_probe.py final
python3 acceptance-bindings/generated_probe.py range-mutant
```

The last command is an intentional negative control with expected exit1. Normal
builds, existing/new tests and comparator checks exit0. Final builds use the same
Release configuration as the earlier caller evidence; new public/Excel/Python
binaries and complete compiler dependency/runtime/configuration hashes are
recorded separately. Unchanged core/math/R3/M5/8daa admission conclusions are
inherited only within their original identities. No full suite, core example,
performance matrix or long financial sampling was necessary for test-only writes.

### Remaining acceptance ledger

Linux generated-entry/portable evidence does not establish native MSVC, XLL,
workbook or 32-bit execution. The eventual new head still needs an actual Windows
`dal_excel_tests` build and execution of both
`ExcelXccyAcceptanceRawTest.TestGeneratedEntryChecksEntireSelectedRange` and
`ExcelXccyAcceptanceBasketRawTest.TestGeneratedRequestChecksEntireSelectedRangeAndTypedInputs`.
The new tests/helper/record and integration require only the original reviewer's
necessary changed-head review and exact-head CI/protected PR gates. No self-merge.
DAL430's public example/documentation remains separately owned and is not imported.

All six original basket profiles remain overall **FAIL**, with unchanged
288/216/214 parity/precision/combined pass counts, original eighteen reports,
FX-sigma precision/combined failures, D80x1 EUR-sigma combined failures, exhausted
x4 reservations, five binary64 FD REDs, full eFD and unsampled native gaps.
The ideal Gaussian tail RED at coordinate339476552, error `6.04063e-10 > 5e-13`,
and `financial_bias_bound_qualified=false` remain. All old raw batches, original
source/binary identities, model/fixtures, counts/budgets/reservations/scales/
thresholds and observed oldHeldOut status are preserved. This callable-interface
checkpoint changes no RNG, estimator or financial evidence and grants no complete
numerical V1, quote-risk or P4 acceptance. DAL425/396 remain in_progress.
