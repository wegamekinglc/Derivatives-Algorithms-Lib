# DAL-406: deterministic curve script families

## State, inputs and ownership

Implementation uses the original `agent/dal-implementer/c097905d2513` branch.
Implementation, merged-master integration and affected verification are complete.
The unique DAL-406 PR awaits the coordinator's focused review and serial shared
documentation window. This is not acceptance or permission to merge/start
downstream work.

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
The merge preserves its RateStrike admission, Gaussian dispatch, compiler/context
identities and tests. Four conflicts in preparation, instrument admission and
instrument tests were resolved additively. No Gaussian/MC/model product or shared
publication file differs from this actual merged master.

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

## Verification and provenance

After master integration: **217/217 affected core** and **47/47 public
compatibility** checks pass. The author adds 24 tests: 20 curve-family, three
instrument admission/persistence cases and one preparation case. Core coverage
includes the existing Deposit/curve adapter, native YCInstrument cross-family,
preparation/instrument/protocol, BondOption, RateOption and MC contract tests.
Public coverage includes curve instrument/protocol/spec and script archive/
contract. Full unrelated product/frontend/statistical matrices are not rerun.

The current source is compiled with GCC 14.3.0, C++17, Release `-O3 -DNDEBUG`,
`-ffp-contract=fast`, Eigen enabled and native-architecture tuning disabled.
The initial standalone core build has public disabled; the integrated final
build enables public and freshly links both clients using their real CMake
compile commands. Examples/Python/Excel/portable Excel are disabled; this PR
does not change those components or build configuration. Exact submodule pins,
CMake cache, source and executable hashes, test filters, commands, XML and logs
are in the delivery attachment. No old executable is relabelled as a new run.

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
The post-integration legacy Deposit spread-unit witness first fails at the wrong
field; the narrow ALPHA-template guard restores the original owned parameter-unit
error and is included in the final green suite. Exact commands, XML and logs are
retained; setup/compiler/harness failures are identified separately from RED.

Nine affected translation units pass syntax checks with GCC `-Wall -Wextra
-Wpedantic -Werror` and the existing Linux workflow's exclusions. Changed
production functions (68) have CCN at most eight. All 13 changed C++ files pass
`clang-format --dry-run --Werror`; documentation/link checks and `git diff
--check` pass. There are no author changes outside the 14 listed paths in the
delivery manifest. All 2,857 inherited paths outside that scope retain the actual
merged-master Git blobs. Of these, 2,852 also match raw blob bytes; the other five
have the existing `.gitattributes` CRLF checkout conversion. All frozen product
sources and shared publication content retain their master content.

A differential preparation probe compares actual merged-master preparation with
this preparation, using the current extended header layout and otherwise the
same current core archive. Deposit/BondOption/Caplet/Swaption each retain nine
byte-identical definition, binding, instrument, compiler, context, fast-plan,
MC-plan, mapping and quote fingerprints. This isolates the preparation delta;
it is not a claim to have run an untouched full master executable. Frozen MC
statistics and accepted kernel performance evidence are reused only for the
unchanged eligible paths. New curve families have no MC qualification.

Reproduction entry points in the attachment, from the task workdir:

```sh
python3 evidence/build_scoped.py
python3 evidence/verify_boundaries.py
python3 evidence/verify_delivery.py
```

`build_scoped.py` recompiles the selected real core/public CMake test commands,
links fresh executables and runs the documented filters. `verify_boundaries.py`
performs the warning and isolated identity checks and builds the caller harness.
Caller timing is a separate foreground step after compilation has finished;
`caller-perf-environment.json` and CSV retain the exact measurement provenance.

### Published-head Codacy correction

The first published head `6e7315aba4f8fba497ad9a3596349af5b9fd7a27` receives
Codacy check `119654203157`: the independent test helper `OriginalTreePrice` has
CCN 14 against limit eight. Its observation mathematics and recursive binding
are extracted into separate test helpers without changing assertions or formulas.
The three helper CCNs are now 5/3/7; every function in that test file is at most
eight. The corrected translation unit is recompiled with the workflow warning
flags, linked into a fresh affected client, and **20/20 family cases pass**.
Production, build and public paths remain identical to the first published head.
The 217 core/47 public suite results, identity comparisons and informational
caller timings from that head therefore remain applicable; they are not
relabeled as new executions after a test-helper-only refactor. Exact follow-up
compile/link commands, XML, annotation and complexity report are attached.
Codacy and other CI results on the new published head remain external gates.

## Actual caller cost baseline

These are single-head informational measurements at the first published head
`6e7315aba4f8fba497ad9a3596349af5b9fd7a27`, with no paired
regression verdict. There is no existing closed benchmark that invokes the new
curve-family representation. A small external harness covers preparation,
combined Price/par evaluation and raw FD/AAD for the IRS, Basis and custom
fixtures used by the numerical tests. Native cached-rate calls are controls,
not equivalent prepare/evaluate operations or a claimed speedup comparison.

One pinned CPU, `DAL_NUM_THREADS=1`, i9-13900HX under WSL2; 20 warmup calls and
10 rounds. Each round uses 100 prepare, 1,000 Price/par or 100 Jacobian calls.
Compilation and correctness checks finish before timing. Median microseconds
per call:

| Caller | Prepare | Price + par | FD | AAD |
|--------|---------|-------------|----|-----|
| IRS | 222.404 | 12.920 | 60.305 | 37.122 |
| Basis | 275.542 | 16.797 | 74.494 | 38.012 |
| Custom weighted/amortizing | 225.667 | 16.715 | 55.453 | 28.100 |

Cached native IRS/Basis rate medians are 0.138/0.180 microseconds. The new
source-owned adapter therefore has visible caller overhead; these timings do
not imply parity with a direct native rate call. Min/median/max and raw rounds
are attached. No portable threshold or two-build regression gate is claimed.
Legacy numerical kernels, FD defaults and frozen budgets are unchanged.

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
