# DAL-406: deterministic curve script families

## State, inputs and ownership

Implementation uses the original `agent/dal-implementer/c097905d2513` branch.
Master integration and final compatibility/caller verification are in progress;
this checkpoint is not acceptance or permission to merge/start downstream work.

The real prerequisite is merged DAL-405 / PR #617,
`072c7da1b3071e71dac4f901451c2345957bd42a`. The initial boundary-only run had one
Deposit control and five admission RED witnesses, not family acceptance; its
evidence remains in attachment `01a1619c-f7c5-7416-b171-cd97b1d20067`.
Coordinator comment `01a161ae-9981-766a-99b5-f6d5e084c5cc` and the updated issue
release the shared preparation, reserved spread admission and two shared tests
to this author. Research baseline `0f1feabee193d0976ab7a5336c9e9e29cb2aa28d`
is historical. No expert or design phase is restarted.

Inputs read: parent research attachment `01a15c20-3723-7ddd-8ef1-c0ac706cdc16`
§§3.2, 5, 7.4, 10–11 and expert threads;
[DAL-398 contract](../api-notes/dal-398-script-contract.md);
[methodology](../../../docs/methodology/script_calibration_contract.md);
DAL-404 acceptance, DAL-405 final control, role/style/test/component/publication
references and relevant native/shared source/tests.

DAL-411 / PR #616 actually merged at 2026-10-22T04:32:14Z as
`f2dfa8e9cb4eb8508bd3425676e90c2cb513ee86`. It is now eligible for the authorized
minimal master integration. Its unmerged product was not used as a baseline.

## Authority and execution

`dal/curve/curvescript.hpp` adds one-time FRA, Swap, explicitly period-forward
OIS and BasisSwap exporters. Settings reuse Deposit's ID, currency, discount
route, positive notional and annuity floor. FRA requires an explicit payment
date. No native product is retained. N/K are the only new-family declarations;
weights, accruals and fractional notionals are source literals. Reserved signed
BasisSpread/ParAffineSpread reuse Coupon and existing persistence. No key, role,
wire field/version or global default is added.

Whole-AST lowering admits complete `N*(coupon expression)` payments, scalar
`+ - * /`, unary signs and the observations below. K-dependent products/divisors
are refused, including `(K-K)*K`. N appears once as the outer amount factor.
MAX/EXP, unknown indices, arbitrary callbacks, control flow and ON do not gain
curve admission. Existing receiver/history guards run first. Named slots retain
their meaning after declaration reorder. Old positive BondPrice and ALPHA
template boundaries remain protected.

The immutable descriptor derives all payments, observations, source coordinates
and coefficient programs from the bound AST. Floating values are evaluated at
each candidate, not frozen at import. A/B yield Price(K) and `-B/A`; actual A is
checked against the declared sign and minimum magnitude at every scalar
evaluation. Either annuity direction is admitted for literal-accrual contracts.
Observed amounts cannot be passed to discount-only Price/Coefficients as fixed
cashflows. AutoFast/RequireFast use deterministic lowering; GenericDeterministic
uses the same admitted representation. An independent original-tree harness
supplies mathematical observations/numeraires, without calling the lowerer.

The existing adapter supplies raw native FD/AAD columns. Direct `Jacobian()`
now rejects nonfinite harvested partials with owned axes/layout/report, as
Evaluate does. Numeric failure cleans the tape; rejected partials are not
successfully returned. These are raw dMeasure/dNativeParameter, not quote risk.
Legacy native solvers, Brent and Gaussian numerical kernels are unchanged.

## Support and refusal matrix

| Family | Enabled | Boundary |
|--------|---------|----------|
| Deposit | Existing origination lender import/adapter | Existing P0 domain |
| FRA | Arrears at/after end or discounted advance at start | Explicit payment required; no intermediate settlement/Future/STIR |
| IRS | Resolved fixed/floating periods and actual payments | Represented observations and same-currency OIS collateral |
| OIS | Explicit single-curve period-forward exporter | No daily compounding/averaging/lookback/lockout/observation-shift claim |
| Basis | Spread-leg PV plus K annuity minus reference-leg PV | BasisSpread units; no FX/XCCY or collateral relabeling |
| Custom | Explicit weights, fractional notionals, dates and signed annuity | Literal N/K template; no arbitrary callback optimizer |

Single-curve forwards use explicit DF(start,end) source observations divided by
resolved index accrual. Calendar adjustment, actual/nominal day-count context,
stubs and actual fixing/payment dates are resolved once at import. Payment is
discounted at the actual event date. Advance FRA retains `1+alpha*L` in its
settlement denominator, rather than moving an arrears amount to start.

Projection uses canonical 3M/6M LCH Libor with concrete starts. Native end and
index accrual must match nominal maturity and actual currency Libor day basis;
mismatched projection stubs/conventions are refused. Canonical route IDs come
from `CurveScriptProjectionRoute`; role, tenor, currency and explicit background
availability are checked. Quarterly/Semiannual display aliases are not emitted
as index tokens. No market fallback is used.

Fixing must not follow payment/start. History and RequireHistorical today need
exact explicit snapshot entries; Model today reads the candidate. Historical
values have zero curve partials. Missing fixing, look-ahead, unsupported route,
uncovered dependency, annuity and numerical failures own available source/row/
quote diagnostics. Identity includes source/binding, fixings, actual Libor day
basis and frozen market/routes. Rebind/context changes require a new plan.

All new families reject ScriptMC even with opt-in. Compiler/MC/statistics/model
product files stay outside this author's changes. Accepted Deposit P0 evidence
is reused; zero-g is not offered as new stochastic/convexity proof.

## Verification checkpoint

Before master integration: **136 affected core checks pass**, including prior
curve adapter, Deposit, preparation, instrument/protocol and new families.
The latest family executable passes **20/20** cases. Final integrated counts,
source/configuration and executable hashes will be recorded in delivery evidence.
No old executable is relabelled as a new run.

Independent piecewise log-DF/cashflow oracles check Price/par against native rate
calls and the original bound tree. Raw FD/AAD columns match independent native
coordinate bumps. Cases include arrears/advance FRA, stub/fixing/payment lag,
separate 3M/6M projections with background discounting, and three weighted,
amortizing coupons without a new YCInstrument type. Native families fit independent
flat-curve targets; projection baskets recover two native coordinates. Values
use 1e-12 and raw partials 1e-8 tolerances.

Price FD checks explicitly use h=1e-5 with independent h/2. The initial default
h=1e-6 showed about 4e-8 amount-roundoff at N=100; tolerance 1e-8, the API default,
legacy FD and every MC scale/sample/budget remain unchanged. The original-tree
harness initially omitted payoff storage initialization; that setup crash was
fixed and is not production RED evidence.

RED/GREEN records cover spread/literal-accrual admission, fixed and observed
cashflows, notional dimension, exporter skeleton refusals, canonical projection
aliases, declaration order, direct nonfinite AAD and helper-token ownership.
Skeleton RED is explicitly a new-API checkpoint, not an untouched old executable.
Exact commands, XML and logs are retained. Compatibility, final warning/format/
complexity checks and caller measurements are being completed after integration;
no unrelated full matrix is claimed.

## Shared documentation drafts

Public guide/CHANGELOG/index writes remain reserved to DAL-411 until the
coordinator releases their serial window. No shared publication file is edited.

Guide draft: add the four exporters and bounded curve arithmetic to the existing
contract guide; cover explicit FRA settlement, period-forward OIS limits,
signed BasisSpread/annuity, nominal projection mapping and the explicit canonical
route recipe, historic/today policy and deterministic-only admission. Show
custom source literals and raw native FD/AAD. Retain DAL-405/P0/411 explanations;
do not imply downstream bindings, XCCY MC or calibrated quote risk.

CHANGELOG draft: bounded FRA/IRS/period-forward OIS/Basis/custom cashflow import
and source-derived Price/par with candidate-curve FD/AAD and owned diagnostics.
Artifact-index draft: link this active record. Existing guide entry links need
no addition unless a separate guide is introduced.
