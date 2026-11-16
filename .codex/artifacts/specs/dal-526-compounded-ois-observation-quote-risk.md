# DAL-526: Compounded OIS observation conventions and same-source quote risk

Status: active implementation control, approved API plan of 2026-11-15.
The plan was approved in DAL-526 and supplied as attachment
`01a1dd8b-d57c-7bd5-8cec-2ae5a476644c`.
Its analysis baseline is `3a466c777dec13cb99b317fedda6f1e69770a9a9`;
stage 0 starts from master `e669ced750972b65011693b11ed828888f1d7248`.
This control specifies the completed feature, not currently available capability.
Stage 0 freezes contracts and passing compatibility fixtures only. It implements
neither `/2` nor OIS quote-risk admission.

The [shared script API decision](../api-notes/dal-398-script-contract.md) controls
ownership, binding, units and errors. Its historical P0 limitations are not the
current capability list. Current behavior is described in the
[OIS guide](../../../docs/ccy-curves/compounded-ois.md) and
[calibration methodology](../../../docs/methodology/script_calibration_contract.md).
No separate OIS critique is active; this spec adopts the approved plan's decisions
without opening another design or critique stage.

## Scope and delivery gates

Use one frozen observation schedule through source construction, native passive
and AAD execution, calibration evidence, snapshot and fixed-contract Price risk.
The first completed release supports same-currency OIS collateral, positive fixed
N, signed fixed decimal K, PayFixed/ReceiveFixed, explicit fixed legs, coupons and
actual payment dates. It supports one observation mode per contract and one
explicit frozen calendar; basis is ACT/360 or ACT/365F.

Retain existing single-target, square, full-rank EXACT root risk infrastructure.
Support an ON target with passive discount, a discount target with passive ON,
and one target serving both roles. Existing admitted Rate/BasisSpread instruments
may share the calibration. Multi-target/joint or staged risk, APPROXIMATE roots,
MC/GSR/XCCY execution, second-order risk and live-quote payoffs stay outside this
capability. Do not loosen rank or accuracy gates to admit an OIS fixture.

One issue and independently mergeable PR per phase, based on merged predecessors:

1. DAL-531 / phase 0: this control, registration, manual schedule, `/1` golden
   archive/source/identities, three square target-role fixtures and passing
   support/rejection baselines. No production changes.
2. DAL-532 / phase 1: typed convention, expansion, frozen mapping and versioned
   archive. First None/Lookback, then Shift/Lockout. Structural preparation samples
   no rates. Downstream unsupported paths remain explicitly rejected.
3. DAL-533 / phase 2: ON2 parsing, source validation and shared native evaluation.
4. DAL-534 / phase 3: same-source calibration evidence and closed frozen context.
5. DAL-535 / phase 4: snapshot and per-PV admission plus complete quote FD oracle.
6. DAL-536 / phase 5: C++/Python/Excel projections and separate core/facade examples.
7. DAL-537 / phase 6: scoped performance, installation and current-state docs.

Complete `/2` support is claimed only after all pricing, calibration and risk
paths for the mode are delivered. Early phases may expose only finished
preparation capabilities with explicit rejection of unfinished execution paths.

## Version selection and source compatibility

The existing `CompoundedOisScriptSpec_::ruleId_` default remains
`dal.compounded-ois-cashflows/1`. With no convention, it still requires complete
explicit observations, including supported nonbusiness-day partial stubs. Its
Spec/Observations metadata, source text, ON literals, canonical JSON, contract
and preparation identities and numerical behavior stay unchanged.

Append optional `observationConvention_` to the C++ spec. Its typed
`CompoundedOisObservationConvention_` contains Machinist-generated
`CompoundedOisObservationMode_ mode_`, `lookbackBusinessDays_ = 0` and
`lockoutBusinessDays_ = 0`. Tokens are exactly None, Lookback, ObservationShift,
Lockout. Mode and wire keys are case-sensitive.

`/1` plus any convention, even None, is rejected. Explicit
`dal.compounded-ois-cashflows/2` requires a convention and empty/omitted input
observations. A nonempty manual table is rejected as dual ownership. Missing
convention, unknown rule/mode, incompatible parameters or misspelled keys fail
explicitly. None has both counts zero; Lookback/ObservationShift require a
positive integer n and zero lockout; Lockout requires positive integer k and zero
lookback. No mixed or per-coupon modes, currency-implied defaults, auto tenor or
payment schedules, spread/floor/rounding, variable N or intraday publication.

Malformed conventions/counts diagnose `/observation_convention/...`, dual table
input `/observations`, and invalid versions `/rule`, retaining instrument/coupon,
expected and actual values. Nonbusiness boundaries or missing Lockout anchors
use UnsupportedCompoundedOisRule or an owning structural-input error, without
automatic adjustment. New archives reject unknown/duplicate fields and mixed
schema/rule versions; binding interfaces preserve their original input locations.

For `/2`, every coupon boundary is a business day in the frozen calendar,
including explicit working-weekend overrides. Do not adjust an invalid boundary.
Keep the original declaration in Spec with observations empty; expose the derived
immutable rows through `ObservationSchedule()` and
`ObservationScheduleIdentity()`. Existing start/end/accrual fields continue to
mean actual accrual, never the shifted weight.

Preserve existing action signatures: PrepareCompoundedOis,
NewCompoundedOisScriptInstrument, EvaluateCurveScriptInstrument,
CalibrateCurveInstruments, NewScriptCurveQuoteRiskSnapshot and
ValueCalibrationInstrumentWithQuoteRisk. Use typed settings, not additional
positional n/k/calendar switches. Preserve source compatibility and aggregate
tail defaults; layout changes require same-version core/public/binding rebuilds,
without an old-binary ABI promise. PeriodForward OIS import remains unchanged.

## Resolved rows and cashflows

For coupon `[S,E)`, let business boundaries be `a[0]=S,...,a[m]=E`.
`B(d,-n)` goes back n business days in the same resolved calendar. Every passive
`CompoundedOisResolvedObservation_` owns actual accrual start/end/accrual, complete
rate start/end/rate_accrual, weight start/end/compounding_accrual, true fixing
index/time, coupon ID, zero-based row, source key and optional reused_from_row.

None samples `[a[i],a[i+1])` and weights by that actual interval. Lookback(n)
samples `[B(a[i],-n),B(a[i+1],-n))`, fixes at its left endpoint, and keeps the
actual interval's weight. ObservationShift(n) uses the same sample and fixing,
but weights by the shifted interval: compounding_accrual equals rate_accrual.

Lockout(k) requires `0<k<m`. Set `j=m-k-1`; the final k rows reuse row j's full
rate interval and fixing key/time, with each row's own actual accrual weight.
The anchor itself remains. Deduplicate samples if useful, but preserve every
factor and accumulate all adjoints. Never replace a product of repeated-rate
factors by a single factor with their summed weight.

```text
L[i] = (P_on(rate_start[i])/P_on(rate_end[i]) - 1)/rate_accrual[i]
G[c] = product_i (1 + compounding_accrual[i]*L[i])
FloatingAmount[c] = N*(G[c]-1)
PV_PayFixed = sum_c DF(payment[c])*FloatingAmount[c]
              - sum_j DF(fixed_payment[j])*N*K*fixed_accrual[j]
PV_ReceiveFixed = -PV_PayFixed
```

Historical L comes from the explicit snapshot. ObservationShift pays the
observation-period compounded return `N*(G-1)`, with **no actual/shifted length
ratio**. Loan-style annualization and restoration to the original accrual period
requires a separate future contract. Fixed accruals and actual payment dates
never shift. Price=B+K*A and ParRate=-B/A retain the signed annuity floor.

The independent [manual schedule](../../../dal-cpp/tests/curve/data/compounded_ois_observation_schedule.csv)
uses a Mar-04 holiday and coupon Mar-05 to Mar-08, 2024, ACT/360. With n=1,
the first sample is Mar-01 to Mar-05: Lookback weight is 1/360, Shift is 4/360.
The shifted period has six days against three actual days. Lockout(1) fixes at
Mar-05, Mar-06, Mar-06 with three separate 1/360 factors. Rows are a hand-written
future schedule oracle, not a claim that `/2` expansion already runs.

## Fixing and payment boundary

Classify by the final mapped fixing date f versus valuation T. Strictly past
f requires explicit history for live payments; missing data is MissingFixing,
never projection. Today RequireHistorical likewise requires history. Today
Model is the unchanged default: project even if the snapshot has a different
today value. Future rows always project, ignoring future snapshot values.
Lockout reuses the anchor's classification, not the tail accrual dates.

Historical rate derivatives are zero, while target discount risk may remain.
Payment<T is settled: validate source/table structure but request no fixing,
rate or DF; do not fabricate an evaluated rate or amount. Payment=T remains
included under IncludeUnsettledAtValuation. All-settled Price is zero and
ParRate unavailable. An all-historical ON-target fit without information must
fail the rank gate.

Preparation freezes structure, classification and request lists without reading
rates or fixing values. Explicit empty snapshots are authoritative; never fall
back to global history/calendar. On actual evaluation, missing history identifies
the true key/date, coupon, row and source location. Deduplicate by complete key
and time while retaining all dependent rows.

Projected queries use the complete rate interval and payments use actual payment
dates. Require coverage only for actual queries, without historical extrapolation
or fictitious knot queries. Before allocation validate date/count arithmetic,
aggregate rows below INT_MAX, source length and scratch capacity. Reuse prepared
business-day ordinals/windows; do not repeat an n-day walk for every output row.

## Source and native execution admission

Preserve legacy ON's `rateStart <= start < end <= rateEnd` invariant. A separate
typed index with configuration-object construction carries the new wire token:

```text
IR[ccy,ON2,fixing_key,route,accrual_start,accrual_end,rate_start,rate_end,basis]
```

ON2 validates each interval's ordering; matched `/2` tables validate their
relationship. Each source factor explicitly contains compounding_accrual.
Lowering compares the entire bound AST with frozen rows: intervals, fixing
key/date, weights, N/K, direction, payments and source provenance, including
settled rows. A token alone never authorizes execution.

Extend ContainsOvernight and equivalent guards to ON2, preserving rejection in
general script/MC and legacy tenor staging. Route complete sample intervals to
the ON provider and independent weights to the shared stable log1p/expm1 program.
Passive/AAD use the same algorithm; reject nonpositive factors, nonfinite values
and cumulative overflow without clipping. Capability/compiler identities
distinguish `/2`; legacy `/1` source/identities remain fixed.

## Same-source calibration and immutable context

Register each calibration or PV contract's prepared tables by unique instrument
ID in `compoundedOis_` **before calibration**, including noncalibration PVs.
All use the spec's valuation date, today policy and full explicit fixing snapshot.
Freeze the complete collection into the snapshot. Match each requested PV's
source/binding/table identity to its own entry; do not borrow the first
calibration table or append a PV during a risk call. A new unregistered PV needs
a new evidenced fit/snapshot. Arbitrary new-PV contexts are a future API.

Reuse existing FreezeOvernightContext and solve evidence, including deep copies
of curves/fixings/quotes and all frozen table identities. Identity closure covers
rule/mode/counts, complete calendar and schedule, weights/reuse, payment status,
valuation/fixings including empty series, source/binding/compiler/capability/plan,
target geometry/native layout/routes/useTargetCurve, complete passive ON/discount/
base curves and coverage, original solve evidence/options/settings, quote IDs,
values/units/order, measures and error scales. Caller aliases and container
replacement cannot mutate published snapshots, results or diagnostics.

Replace both blanket guards in `dal-cpp/dal/curve/scriptcurvequoterisk.cpp` only
after verified `/1` and `/2` capability/table/native-context admission is ready.
Do not merely delete ContainsOvernight. Missing tables, wrong versions, modified
bindings and stale context remain rejected, without generic-evaluator fallback.
Require original successful EXACT evidence, equal independent quote/observation/
native-coordinate counts, recomputed raw residuals and AAD J, full rank, reciprocal
condition/relative pivot and response/transpose backward-error checks. Optional
quoteJacobian or a fit success flag cannot substitute for evidence.

## Risk units and independent oracle

For fixed-contract Price, V_q=0; market q never rebinds K, N or source. With
`R(theta,q)=F(theta)-Q*q`, use the existing checked factorization:

```text
J = dF/dtheta; J*M = Q; M = dtheta/dq
g = dV/dtheta; J^T*lambda = g^T
raw = dV/dq = Q^T*lambda
DV01[j] = raw[j]*0.0001
```

Rate/BasisSpread q is decimal (2%=0.02, 1bp=0.0001). Raw units are result
currency amount per 1.0 decimal rate; DV01 is signed currency PV change for an
upward 1bp quote move. Keep actual N and direction, with no absolute value,
extra minus sign or per-million/per-100 normalization. Doubling N doubles
Price/raw/DV01; ReceiveFixed reverses their signs. Distinguish native partials,
parameter response and quote risk in all labels. Do not rescale residual or
parameter scales twice, invert J explicitly, or differentiate solver iterations.

Full mapping and pvOnly=true share admission; PV-only may have RiskAvailable
true and MappingAvailable false. Every PV must check its own availability and
transpose error. Preserve calibration-report/1 and QuoteRiskAvailable semantics.
Unavailable owns diagnostics, not a zero risk vector. ParRate risk, wrong units,
unknown/duplicate IDs, stale context, source tampering and liveQuotePayoff fail.

For each admitted rule/mode and target role, independently recalibrate every
quote at q[j]+/-h (h=1e-4 and 5e-5). Change only q; independently check both
roots/rank/condition. Reprice the same immutable fixed-K contract using direct
native evaluation and rebuilt fitted curves, without the risk endpoint or reused
response matrices. Keep passive curves/calendar/fixings fixed. Compare raw with
central PV difference/(2*h), and DV01 with that result times 1e-4. A one-sided
1bp PV change includes curvature and is not the exact derivative.

Well-conditioned fixtures use raw Rate residual <=1e-12, reciprocal condition
>=1e-10, and response/transpose backward error <=1e-12. After dividing by N,
raw error and two-step FD stability must be <=`1e-7+1e-5*abs(FD_raw/N)`;
DV01 equivalents are `1e-11+1e-5*abs(FD_DV01/N)`. These are fixture thresholds,
not production defaults. Improve root accuracy/conditioning before considering
any documented limitation; never automatically relax them. Include LOG_DISCOUNT
and ZERO_RATE positives and unobservable PWLF/PWC rank negatives.

## Persistence, bindings and audit

Preserve preparation/1 and cashflows/1 readers/writers exactly, without writing
default convention fields. New preparation/2 contains `/2` declaration,
convention, frozen calendar/valuation and all resolved rows. On load re-expand
the authoritative declaration, compare every stored derived field, then rebuild
identity/status; never trust incoming identity strings or downgrade to `/1`.
ContractIdentity includes declaration and normalized rows; schedule identity
includes calendar/rule/convention/rows; preparation adds valuation/fixings.
Definition/binding archive schemas may remain unchanged with explicit ON2.
Parser, clone, serialization and UTF-8 percent encoding round-trip; old readers
reject new rules/tokens explicitly. Snapshots/results remain runtime owning
objects without new persistence or cross-process session support.

Public C++ exposes the typed convention and immutable schedule, with
CompoundedOisObservationScheduleTable. Python adds a typed convention constructor
with keyword counts, optional spec observation_convention plus explicit rule_id,
read-only observation_schedule and observation_schedule_identity, and
CompoundedOis_Get_ObservationSchedule. Preserve action names/order and old defaults.
Excel adds COMPOUNDEDOISOBSERVATIONCONVENTION.NEW and
COMPOUNDEDOIS.GET.OBSERVATIONSCHEDULE, with optional convention handle/version
inputs at the spec tail. Do not change the existing seven observation columns.

Share strict rule-specific conversion in the public component. Python bool is
not numeric; check exact integer ranges, copy inputs before releasing the GIL,
and return independent containers. Excel validates entire selected regions,
unknown/duplicate keys and bad handles, retaining worksheet row/column errors.
Missing fixing/numerical failures retain owning outcomes; unavailable numeric
cells stay blank and getters reject them.

The new prepared and evaluation observation_schedule selectors own rule/mode,
coupon/zero-based row/source key, actual/rate/weight intervals and accruals,
fixing key/time, historical_required, included/settled and reused_from_row.
Evaluation additionally reports Historical/Projection/Settled and optional rate.
Keep old observations selector and columns. Old source keys stay
`observations/<zero-based-row>`; new keys are
`observation_schedule/<zero-based-row>`. Document index bases explicitly.

## Acceptance and verification scope

Stage 0 fixtures live in `dal-cpp/tests/curve/compoundedoisfixture.hpp`,
`test_compounded_ois_baseline.cpp` and `data/compounded_ois_v1_*.json`.
Golden files freeze master `/1` prepared JSON, full instrument archive (including
exact source/binding) and six identities. Generated-versus-generated round trips
alone are insufficient. Three two-coordinate square fixtures use independent
scalar quotes: ON/passive discount, discount/passive ON (Deposit anchor plus
two-payment OIS), and shared ON/discount. They preregister a distinct off-market
PV. Snapshot rejection is checked on actual solved OIS fits; per-PV rejection
uses an otherwise available Deposit snapshot, identifying both live guards.
These rejection assertions are temporary compatibility gates, to be replaced by
positive admission tests in the implementing phase; do not retain blanket
rejection after capability delivery. `/2` rejection is likewise a phase 0 gate.

Later acceptance covers the manual calendar rows, working weekends, leap days,
consecutive holidays, business-boundary stubs/multiple coupons, both bases,
invalid parameters/boundaries/count/date overflow, missing history, today policies,
future snapshot isolation, settlement, negative/tiny rates and overflow. Compare
source/passive/AAD with independent products and two-step native FD; repeated
fixing adjoints must accumulate. Verify old numerical/identity defaults and new
archive round trips/tampering, lifecycle/alias isolation and three binding surfaces.

Follow [component boundaries](../../references/component-boundaries.md): existing
core examples stay under dal-cpp with core APIs; separate facade programs go in
dal-public/examples. Example/build changes require actual public-disabled core
build/run and independent facade build/run plus installed-consumer checks.
Do not claim Windows XLL/session/spill validation from portable Linux tests.

Follow the [performance workflow](../../references/benchmark-workflow.md).
Phase 0 changes no production or build paths, so timing, examples, full product
suite and cross-platform runs are outside its local validation scope. Run content
checks and affected baseline suites; full matrices remain existing CI work.
Subsequent schedule/kernel changes select compounded-OIS D=30/365/1825 with fixed
node count and cross-holiday/lockout shapes. Snapshot/per-PV changes select
script-curve quote-risk full and PV-only at 5 quotes/1 PV, 15/1 and 15/100;
parser/provider changes also check affected `/1` and Deposit/IRS/Basis neighbors.
Retain cold/prepared/AAD/scratch/tape measures, binary/source/dependency/config
provenance, two rounds of ten alternating process pairs, best-of-ten minima,
4% thresholds and raw samples. Preserve linear product/adjoint traversal, no
D-by-D matrices, and shared checked factorization. Do not call total schedule
preparation O(D) without accounting for sort/calendar/dedup costs or compare an
O(D) product to O(1) PeriodForward as a regression baseline.

Final acceptance requires all seven merged stages, same-source quote FD and
units, strict rejection/identity/accuracy gates, compatible three-end APIs,
examples, installed consumers, scoped performance and updated current docs.
Combination modes, loan-style shift, nonbusiness partial `/2` intervals, live
publication feeds, arbitrary unregistered PVs and multi-target risk remain
separate future contracts.
