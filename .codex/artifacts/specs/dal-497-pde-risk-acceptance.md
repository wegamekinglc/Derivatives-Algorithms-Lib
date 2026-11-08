# DAL-497 fixed acceptance fixtures and performance gates

Status: active S0 acceptance contract, version 1. All product checks below
are **NOT EXECUTED** at S0. These are requirements, not measured results.
Read together with the [R1–R11 contract](dal-497-early-exercise-pde-risk.md)
and [API schema](../api-notes/dal-497-early-exercise-pde-risk.md).
Source baseline is `4e9eb5eb7cae770826e6f4e0a05aeb9bf4c4c44e`.
The plan attachment's section 9/11 thresholds are now frozen. A failed
qualification requires repair or an explicit issue decision, never a looser
post hoc threshold, a removed fixture or an unavailable smooth risk.

## Metrics and evidence

Use absolute differences of the normalized vector
`(V/K, Delta, K*Gamma, Vega/K, Rate[j]/K, DividendYield[j]/K)`.
Optional/failed values cannot pass an error test. Relative errors are
supplementary only. Product and refinement comparisons include equality;
reference self-qualification is strict as stated below.

| Metric             | Product/reference hard limit | Last refinement limit | Reference self-qualification limit |
|--------------------|------------------------------|-----------------------|------------------------------------|
| Price / K          | 1e-4                         | 5e-5                  | 2.5e-5                             |
| Delta              | 2e-3                         | 1e-3                  | 5e-4                               |
| K × Gamma          | 2e-2                         | 1e-2                  | 5e-3                               |
| Vega / K           | 3e-3                         | 1.5e-3                | 7.5e-4                             |
| Rate[j] / K        | 3e-3                         | 1.5e-3                | 7.5e-4                             |
| DividendYield[j]/K | 3e-3                         | 1.5e-3                | 7.5e-4                             |

Reference self-qualification uses strict less-than, as in the accepted plan:
each last refinement difference, tree odd/even difference and reference bump
halving difference must be < one quarter of the hard limit. Exact analytic
references have no mesh error; validate their formulas separately with an
independent scalar implementation. A reference not meeting its own limits
means INCOMPLETE, not product PASS or product FAIL by that reference.

Store every fixture's original/resolved settings and IDs, source/dependency
SHAs, build flags/toolchain, executable/library hashes, reference identity,
all requested risk/scenario rows, original LCP residuals, step differences,
work/capacity data and raw comparison/convergence tables. Keep FAIL and
INCOMPLETE rows. Historic #602 prices or European AAD evidence do not fill
these fields. Run only cases affected by each stage and reuse applicable
fixed-identity evidence; the final collection must cover all N/B requirements.

## Deterministic profiles

All unspecified price settings use their existing defaults, including theta=1,
Rannacher request=2, penalty=1e8, abs tolerance=1e-8, relative tolerance=1e-10,
linear error limit=1e-12, max iterations=100. All unspecified risk settings
use the new frozen defaults. Default risk switches are all on. There are no
random draws or adaptive fixture searches. D0=2026-10-18, E=2027-04-18,
C=2027-03-04, D1=2027-10-18. K=100.0, sigma=0.20 unless overridden.
Single carry `(r,q)` always ends exactly D1. PUT/CALL are explicit per case.

| Profile | N    | Upper | Ordinary steps | Intended use                          |
|---------|------|-------|----------------|---------------------------------------|
| D       | 401  | unset | 500            | Product defaults / failure / bindings |
| Q       | 1601 | 400   | 8000           | Smooth qualification                  |
| C       | 401  | 400   | 5000           | Explicit CN, theta=0.5                |
| L       | 7    | 400   | 8              | Exhaustive small LCP                  |

D retains default product budgets. Q, C and L use the following explicit
qualification budgets unless a row overrides one: valuation max_actual_steps
200000, max_work_units 1000000000, numeric_payload_budget_bytes 1073741824,
workspace_budget_bytes 1073741824; risk max_scenarios 129, max_work_units
10000000000, numeric_payload_budget_bytes 2147483648,
workspace_budget_bytes 2147483648. These fit 64-bit size_t and exact Excel
integers. They are not new product defaults or an entitlement on 32-bit hosts.
Unsupported address space is INCOMPLETE with the platform recorded.

The larger caps account for American worst-case region envelopes at N=3201
and 8000 steps, plus one simultaneous bump result. They do not promise that
every allowed input fits. Preflight still applies; exceeding these fixed
profiles is a reported blocker, not an automatic budget escalation.
Work is not an iteration shortcut; no residual/tolerance relaxation is allowed.

Every full-risk smooth one-segment fixture plans 17 scenarios; every full-risk
two-segment fixture plans 25. Explicit upper stays fixed across bumps even
when S>K. Product default upper is tested separately. Fine spot h=2dx unless
specified, other coarse bumps 1e-3/1e-4/1e-4. Report both scales, all risk
statuses and actual rather than requested time counts.

## Independent references

ANALYTIC: compute discounted BS with R=sum(r_j*tau_j), Q=sum(q_j*tau_j),
T=(maturity-evaluation)/365 and sigma√T. Standard normal uses independent
erfc/exp evaluation, not DAL pricing or risk functions. For call:

```text
d1=(ln(S/K)+R-Q+0.5*sigma*sigma*T)/(sigma*sqrt(T)); d2=d1-sigma*sqrt(T)
V=S*exp(-Q)*Phi(d1)-K*exp(-R)*Phi(d2)
Delta=exp(-Q)*Phi(d1)
Gamma=exp(-Q)*phi(d1)/(S*sigma*sqrt(T))
Vega=S*exp(-Q)*phi(d1)*sqrt(T)
Rate[j]=tau_j*K*exp(-R)*Phi(d2)
DividendYield[j]=-tau_j*S*exp(-Q)*Phi(d1)
```

For put use parity and its derivatives: price add Kexp(-R)-Sexp(-Q),
Delta subtract exp(-Q), Rate[j] subtract tau_j*Kexp(-R),
DividendYield[j] add tau_j*Sexp(-Q); Gamma/Vega unchanged.
Deterministic sigma=0 references maximize discounted intrinsic over exact
exercise opportunities, also stationary points within American carry segments.

TREE: independently implemented constant-carry CRR, not the product rollback
or DAL test-support PDE. u=exp(sigma√dt), d=1/u,
p=(exp((r-q)dt)-d)/(u-d); discount exp(-r*dt). American obstacle at every tree
layer, European only terminal. Use paired n/n+1 at n=4096,8192,16384.
Record both raw members and their arithmetic mean per level; final estimate
is the final pair mean, admitted only after raw odd/even and last pair-mean
differences satisfy the reference limit for every metric. Never silently
choose the parity closer to product.

For reference finite differences use the product's absolute coarse/fine
parameter bumps, plus one extra halving of the product fine bump. Reprice
every node scenario independently. Report the final pair mean derivative at
that extra half bump; its difference from the product fine bump derivative
must pass reference self-qualification. For spot this is allowed in the
reference even though product spacing admission differs. The fine-scale
product remains compared against an independently qualified derivative.

LOG-LCP: independent log-price discretization for piecewise carry and dated
Bermudan/American. Use x=ln(S/K), bounds [-4,4],[-5,5],[-6,6] with respectively
4097,8193,16385 nodes; steps per calendar-day interval 16,32,64, so event dates
are exact. Use implicit stepping, independent tridiagonal assembly/solve and
projected SOR for American (omega=1.2, original-LCP price residual 1e-11,
iteration cap 100000). Include evaluation, every calendar day, carry/exercise
events and maturity as anchors. Endpoint envelopes are independently computed
discounted intrinsic maxima over eligible exercise opportunities, with
American stationary candidates. Use local quadratic interpolation at query
spot (three neighboring nodes, no DAL interpolation helper). Apply the same
reference bump/halving protocol as TREE. Each last pair of levels must pass
the reference limit, including Greek domain sensitivity. No shared DAL
planner, theta/penalty, boundary, interpolation or active-set helper.

TREE is mandatory for constant-carry N3; LOG-LCP is mandatory for N4/N12 and
piecewise smooth stress. Shared `dal-cpp/test-support/bermudan_pde.hpp` may
cross-check but cannot be the sole independent oracle. Fixed reference levels
that fail self-qualification stop that qualification; expansion requires
recorded evidence and an explicit follow-up decision, not a hidden retry.

### Approved bounded TREE continuation (DAL-502)

Coordinator decision `01a1b884-3818-7d26-bb9a-2493636a2df9` authorizes an
append-only continuation after the original TREE INCOMPLETE result. Keep all
4096/8192/16384 evidence. Append paired 32768/32769 and 65536/65537 in order;
both are mandatory. If the latest level still fails self-qualification, only
131072/131073 may follow, then stop. No favorable intermediate level/parity
selection, smoothing, replacement oracle, fixture change or threshold change.

The independent reference budget is cumulative 3e12 TREE terminal/rollback
node calculations, at most four workers, RSS 128 MiB per worker / 512 MiB
total, raw output 64 MiB and wall time 90 minutes. A tree with n steps costs
(n+1)(n+2)/2 nodes. Charge every actual cache miss, including equivalence
witnesses and missing old-level scales; do not reset the budget per stage.
Product Q/C/L caps remain unchanged. Cache identities include physical inputs,
axis, absolute offset, n/parity, source, configuration and executable. Reuse of
an older identity needs an explicit recorded equivalence bridge, preserving
the original result's source identity.

Each actual product coarse/fine/extra-half triple needs its own full-vector
self-qualification, including Vega. At the latest layer, all three raw parity
differences, the last adjacent pair-mean refinement and fine/extra-half
difference must be strictly below the original quarter limits before a product
comparison can pass. Missing/nonfinite rows, any exhausted resource budget or
failure at the maximum level remain INCOMPLETE with the raw data retained.
No further layer or budget increase is implicit in this decision.

### Approved BBS diagnostic pilot (DAL-502)

Decision `01a1b8ef-6fc2-7ebd-bda6-d984fc11b161`, after the direction/P2
delivery `d96a89e8d438eb06c69ac3a4b1884ce64b286099`, permits one opt-in
`BBS-last-step-v1` diagnostic. Preserve CRR u/d/p and discount. At layer n-1,
initialize each node with independent scalar European BS for remaining dt,
then max with intrinsic for American; apply the usual American obstacle in
all remaining rollback layers. European controls omit the obstacle. No
Richardson, new pure-CRR run, production change or old-price identity bridge.
The original CRR implementation, values, maximum and INCOMPLETE stand.

The method follows the penultimate-node construction in
[Chung--Shackleton (2002), pp.145,149](https://scholars.lib.ntu.edu.tw/bitstreams/7e27fcda-7bd5-4c3d-baff-5e2dcfff545e/download).
That study evaluates European Greeks; it does not qualify these American
fixtures. This is not authorization to replace mandatory S3 TREE evidence.

Freeze before any new numerical execution: K=100, sigma=.2, T=365/365,
r=.05 throughout. Pilot markets are N3 S=100 American PUT q=0 and CALL q=.08.
Use spots 100, 100±1, 100±.5, 100±.25 independently at n=4096/4097,
8192/8193,16384/16385 (84 jobs). European price controls use the same two
markets with American disabled at those six n (12 jobs), checked against
independent Python scalar BS with normalized price error strictly <2.5e-5.

Freeze formula witnesses: n=1 American PUT S=100 and S=50, American CALL
S=150 q=.08, European PUT S=50 and CALL S=150 q=.08; n=2 American PUT/CALL
at S=50/150 respectively and both at S=100 (nine jobs). One-layer values
must match max(intrinsic, scalar BS(T)) for American and scalar BS(T) for
European. Two-layer values must match the explicit discounted u/d weighted
penultimate formula followed by the root obstacle. Normalized formula
difference <=1e-12. Preserve terminal/root exercise witnesses, and positive
premium/strict continuation for each unbumped pilot price at every n.

Freeze eight expected-failure witnesses: n=0, n=16386; n=1 spot=0, vol=0,
two carry segments, invalid probability (vol=.01,r=1), expired deadline=1;
n=2 CALL spot=1.75e308 for a nonfinite penultimate spot. Other parameters
are the PUT pilot defaults except that CALL uses q=.08. Expected failures
publish no price and retain partial operation counts; they cannot supply
reference values. Unexpected errors/nonfinite/missing results stop admission.

New limits: 6e9 conservatively charged nodes, 2e6 BS evaluations (including
scalar verification), four workers, RSS128 MiB each/512 MiB total, raw64 MiB,
numerical wall600 s. Charge (n+1)(n+2)/2 for each valid-range tree invocation
and reserve n terminal BS calls, including early-failure witnesses; invalid-n
preflight witnesses cost zero. Also record actual visited nodes/BS calls.
Reserve 100 scalar verification calls and count each actual call. The fixed
native plan reserves 5,638,979,836 nodes and 917,572 BS calls; no native
pricing occurs outside it. Compile-only tests cost zero. Old consumed work
2,997,881,833,992 remains separate and immutable: total upper bound
3,003,881,833,992. No old residual allocation may restart pure CRR.

At the final 16384 pair, all three scales' normalized price/Delta/K*Gamma
raw parity and last adjacent pair-mean mesh differences, plus fine to
extra-half differences, must be strictly below [2.5e-5,5e-4,5e-3]. Preserve
both raw members, fixed arithmetic means and every level. Any resource
exhaustion or final lack of self-qualification is INCOMPLETE and stops this
pilot without another level or favorable intermediate selection. Witness
failure is FAIL and prevents admission. PASS establishes pilot feasibility
only; full risk vectors/all bindings and S3 substitution need another decision.

## N1 — European degeneration

Cross product: PUT/CALL; S=80,100,120; q=0,0.03; r=0.05; D0/D1;
Bermudan dates=[D1]; Q profile. All five risk families must be available,
COMPLETE, and pass ANALYTIC thresholds. Carry duration is exactly 1 year.
No early-exercise region is expected before terminal. Price-only agreement
alone is insufficient. Also run one default-upper case S=120 CALL with D
profile to assert upper=480 and identical scenario upper; numerical qualification
for this coarse default-only case is not substituted for the Q matrix.

## N2 — American non-dividend call

S=80,100,120; CALL; r=0.05,q=0; D0/D1; American empty dates; Q profile.
The base and scenarios with nonpositive yield must have empty nonterminal
positive-intrinsic exercise regions; do not infer this from zero payoff.
Price and every risk must be available and meet ANALYTIC limits at the base.
Check one two-segment variant S=100: [(C,0.02,0),(D1,0.06,0)], against
ANALYTIC segment derivatives; both base rates remain nonnegative under bumps.
Do not demand empty regions for positive-q bumped scenarios if their own
economics permits a far-domain contact: the empty-region assertion applies
to the q=0 base and scenarios with nonpositive yield only.

## N3 — genuine exercise premium, continuation risk

Mandatory Q fixtures at S=100 and 120: American PUT r=.05,q=0; American CALL
r=.05,q=.08. Empty exercise schedule, D0/D1. Use qualified TREE reference.
Every requested risk at these continuation points must be available and meet
all hard limits; unavailable is a failure, not an exclusion. Reference and
product must both have a positive early-exercise premium over ANALYTIC
(strictly >0 in normalized units) and evaluation membership CONTINUATION.
If a supposedly smooth reference is not qualified, report INCOMPLETE with raw
evidence; do not replace S or q to make it pass.

## N4 — Bermudan events and individual carry segments

Q; PUT/CALL; S=100,120; [(C,.02,.01),(D1,.06,.03)]; Bermudan [E,D1].
Repeat with first carry end moved from C to E, rates unchanged. Every risk
must be available and pass LOG-LCP limits, including each of the four carry
partials separately. Exact anchors contain C/E/D1 as applicable; a coincident
carry/exercise event is one node with both flags. No step crosses a knot.
Rollback to an event uses the segment to its right, then projects at the
event; preceding rollback uses the segment to its left. Do not coalesce the
segments or compare only their parallel rate sum.

## N5 — dates and ordering

N5a: sigma=0, S=80, PUT, Bermudan [C,D1], Q. Carry first (-.2,-.2), then
(.2,.2), ending C/D1. Repeat swapping parameter pairs without changing dates.
Expected first value is `20*exp(.2*137/365)` (exercise at C); swapped value is
`20*exp(.2*(365-2*137)/365)` (exercise at maturity). Maturity-only uses
`20*exp(-sum(r_j*tau_j))`. Risk rows all degenerate-unavailable, price meets
the normalized hard limit. American empty-date variant uses deterministic
continuous opportunity reference and preserves exact carry order.

N5b: evaluation 2028-02-28, maturity 2028-03-01 (2 actual days), S=100,
sigma=.2,r=.05,q=.03, PUT/CALL, Q with N=16001 (dx=.025), upper=400.
Bermudan [maturity], ANALYTIC T=2/365,
all risks available. Repeat evaluation 2027-02-28, maturity 2027-03-02
(also 2 days). No leap-year denominator 366. For very short payoff kinks,
add half-cell S=100.0125 and retain status/estimates; this is a short-horizon
diagnostic case, not an excuse to exclude the mandatory node fixture above.

N5c: evaluation exercise, S=50, PUT, D0/D1, r=.05,q=0,sigma=.2, Q,
Bermudan [D0,E,D1], and American empty dates. Expected deep intrinsic
price=50; if published Delta=-1,Gamma=0,Vega=Rate=Yield=0 within hard limits,
all records AVAILABLE_EXERCISE_REGION. Require complete here; no arbitrary
unavailable exemption. Maturity-only and American empty-date rules are also
validated as distinct request contracts.

## N6 — separated spatial and temporal refinement

Fixtures: N1 S=100 PUT q=.03; N3 S=100 PUT and S=100 high-yield CALL;
N4 S=100 PUT (noncoincident events). Budgets Q throughout, implicit theta.
Space sequence N=401,801,1601 with upper=400 and fixed ordinary_steps=8000.
Time sequence ordinary_steps=1000,2000,4000 with fixed N=1601, upper=400.
Use default h=2dx at every space level; same h=.5 across time levels.
Record all levels and all risk estimates; each direction's last normalized
difference must be ≤half hard limit and its finest result pass its qualified
reference at the hard limit. No monotonic or global second-order claim.
Final mandatory smooth levels must be COMPLETE; intermediate unavailability
is recorded and means that directional convergence is not established.

## N7 — domain, penalty and bump isolation

Use the four N6 fixtures with Q budgets and steps=8000. Domain sequence
(upper,N)=(400,1601),(600,2401),(800,3201) keeps dx=.25 and h=.5.
Penalty sequence 1e7,1e8,1e9 uses fixed (upper,N)=(400,1601). For Bermudan
the penalty setting is inert: equality is the control. Each American step
must independently pass its original LCP tolerance at every penalty level.

Spot-bump sequence h=4dx then 2dx, each with its own 2h coarse stencil, at
N=801 and then 1601, upper=400; record both changes without mixing them.
Vol/rate/yield configured coarse bump sequences are
(1e-3,1e-4,1e-4), (5e-4,5e-5,5e-5), (2.5e-4,2.5e-5,2.5e-5).
Hold space/time/domain/penalty fixed for each such sequence. Each final
normalized difference is ≤half hard limit, all final risks available, final
values pass the qualified reference. An LCP failure is a real failure of that
sequence; do not omit the low-penalty point or relax epsilon.

## N8 — opt-in CN

C profile with theta=.5, Rannacher request=2: N1 S=100 PUT q=.03 and
N3 S=100 American PUT. Use fixed common upper=400, full risk requests.
Compare against their qualified ANALYTIC/TREE references and Q implicit
results, each at the hard limits. Require all risks available, original LCP
residuals and linear limits admitted, exact shared event arrays, actual
subdivision/damping/storage/work reported within the fixed C budget.
CN failure cannot be cleared by switching to implicit or testing price only.

## N9 — original LCP and deterministic failures

N9a: L profile, American PUT, S=100, sigma=.2, single (.05,0), D0/D1;
only Vega enabled (five scenarios). Independently assemble each unpenalized
boundary-eliminated small system and enumerate all 2^(N-2)=32 active sets.
At every successful step compare the value layer against the feasible
enumerated solution within epsilon plus 1e-10*K. Independently recompute
iteration change, obstacle, dual and complementarity residuals; all four
≤common epsilon, linear backward error ≤configured limit. Test support may
observe internal layers for this check without adding them to the public API.

N9b: use the existing deterministic failure construction, explicitly fixed:
D0/D1, American PUT, S=80,sigma=0, N=41, upper=400, steps=500,
carry [(C,.1,.1),(D1,-.01,-.01)], Q budget overrides. penalty=1 must yield
base LCP_RESIDUAL_LIMIT and aggregate BASE_FAILED; penalty=1e8 is the
successful price control. With penalty=1e8,max_iterations_per_step=1 require
MAX_ITERATIONS. Degenerate risk rows do not hide base failure. Failed
base Price/Values and all risk values are unavailable; diagnostics survive.

N9c: D profile, N1 S=100 PUT maturity-only, linear limit=1e-30, Q byte/work
caps, all risks on. Require LINEAR_SOLVE_FAILURE with measured error >limit
and no failed price; if this limit does not trigger in a supported toolchain,
the numerical-failure stimulus is INCOMPLETE and must be reported, never
pretend failure or replace arithmetic. For deterministic propagation tests
use a private test solver seam that returns the same failure at scenario
`Rate[0]:F+`, first solve, without altering the public request. Base and
unrelated risks remain readable, Rate[0] unavailable, aggregate PARTIAL.
This seam qualifies bookkeeping only, not numerical robustness.

N9d work limits use the exact arithmetic construction under B6 below,
covering base failure, partial completion and a stop during an attempted bump.

## N10 — boundaries, degeneracy and stress

N10a: Q American PUT S=100, single (.05,0), D0/D1. A deterministic contract
test first obtains its base evaluation contact runs, selects the right edge
of the leftmost positive-intrinsic unclipped run, and sets query S to the
midpoint of that edge's adjacent node bracket. Upper/N/parameters/epsilon
stay fixed. Query S is below K, so common epsilon is unchanged. This derived
boundary fixture is for contact-policy testing, not a numerical oracle.
Use h=.5. Require Delta/Gamma near-boundary unavailable, price available.
Also query at bracket midpoint±4h. Independently inspect all scenario
brackets: outside-intersection points must follow R8 membership/stability,
inside/crossing stencils must give the exact unavailable reason. Failure to
find the specified run is a failure of this fixture, not a search for another.

N10b: repeat N1 S=100 PUT at sigma=0 and 1e-8. At zero require only base,
all risk rows UNAVAILABLE_DEGENERATE_DIFFUSION, PARTIAL after base success.
At 1e-8 with volatility_bump=1e-3 require Vega DOMAIN_BOUNDARY; other rows
must expose actual screened results, no blanket false zero/COMPLETE.
N10c domain cases use S=.25 and S=399.75, N=1601,upper=400,h=.5,
maturity-only PUT; Delta/Gamma DOMAIN_BOUNDARY while other axes proceed.

N10d stress matrix (Q, D0/D1, maturity-only Bermudan PUT/CALL):
(S,sigma,r,q)=(20,.2,.05,0),(300,.2,.05,0),(100,1.0,.05,.03),
(100,.2,-.05,.03),(100,.2,.20,-.10). Require every risk available and
ANALYTIC hard limits; use upper=800,N=3201 for sigma=1 only, preserving dx.
Deep intrinsic exercise uses N5c. Upwind-switch control uses S=100,r=.05,q=0,
sigma=sqrt(.05), N=401,upper=400,steps=8000, Q caps, maturity-only PUT.
Volatility stencil straddles the first interior row's central/upwind threshold;
record upwindChanged and apply the usual screen, not automatic failure.

## N11 — Gamma interpolation trap

N1 PUT q=.03; Q at S=100 (node) and 100.125 (half cell). Supply explicit
spot_bump=.125 (<dx) and require InvalidEarlyExercisePdeRiskRequest before
rollback. Then explicit .5=2dx, all risks on, finite-scale method/interpretation,
nonzero Gamma and ANALYTIC hard limits. Repeat space sequence N=401,801,1601
using node S=100 and half-cell S=100+dx/2 at each level, default h=2dx,
fixed steps=8000. Last normalized Gamma change ≤.01 and final analytic
error ≤.02. Record h/2h and weights; no micro-bump zero counted as success.

## N12 — carry axis audit

N4 S=100 PUT, both knot variants, Q. Canonical output has seven records:
Delta,Gamma,Vega,Rate[0],DividendYield[0],Rate[1],DividendYield[1].
Each carry record's dates/parameter value and each scenario offset affect
exactly one original segment. Require each risk available and independently
qualified LOG-LCP agreement. Repeat with coarse rate/yield bumps 1e-4,5e-5
(fine=half); independently parallel-bump both rate segments and then both
yield segments using the same fine step. At the smaller step, normalized
abs(sum(segment partials)-parallel partial) ≤3e-3 for each family, and
parallel reference's own bump halving difference <7.5e-4. Do not relabel as
quote risk. Individual selector combinations retain filtered canonical order;
Delta/Gamma share scenarios. Test all-off rejection and incorrect vector lengths.

## B1–B10 — binding, resource and build controls

Use a shared persistent construction of N1 S=100 PUT q=.03 with D profile
for schema/lifetime cases; arithmetic equality does not qualify its accuracy.
Use the exact N9/B6 cases below for COMPLETE/PARTIAL/BASE_FAILED/WORK_LIMIT
projections, keeping the same compiled native library where possible.

| ID  | Required checks and pass condition                                                                                                                                                                                                                                                            |
|-----|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| B1  | Destroy/change source requests; C++ copy/move, Python copy/deepcopy, detached nested/list getters, Excel source-handle release/table mutation. Values/settings/axis/diagnostics unchanged; repeated getters add zero solves/work.                                                             |
| B2  | Existing recording, finished graph awaiting Reverse, legacy graph and multiple adjoint channels; tape node/event counts, recording mode, channel count, seeds/adjoints unchanged and subsequent Reverse equals control. Concurrent distinct requests retain separate plans/budgets/results.   |
| B3  | Python strict flag/real/count/date/type/sequence/keyword inputs and each bound; readonly and detached data; exact error prefixes. GIL heartbeat coordinated by barrier advances during native compute, ThreadPoolExecutor agrees with serial results. No sleep-only assertion of GIL release. |
| B4  | Excel optional blanks, duplicate/unknown/case keys, half-empty inputs, integer/date limits, all new table headers/order, unavailable blanks/scalar errors, atomic existing-handle replacement, archive rejection.                                                                             |
| B5  | Core/public/Python/Excel status/labels/units/dates/plan arrays/counts identical on one native build; finite scalars differ by ≤1e-12*max(1,abs(reference value)). Across toolchains use schema equality and N limits, not bit equality. Cover all four aggregate states.                      |
| B6  | Scenario/work/byte caps at exact boundary and one less; zero solve; checked overflow; smaller underlying valuation limits. No rollback after preflight rejection, no work overshoot, actual capacities within envelopes. Exact constructions below.                                           |
| B7  | Repeated malformed/preflight/numerical failures followed by success; private scenario failure seam N9c; global work interruption. No stale values; completed independent risks survive; all unrun rows explicitly classified.                                                                 |
| B8  | Fresh build with public, Python, Excel and Excel portable OFF, core examples ON; affected core targets build/run. Existing example directories/targets/CTest/install names retained; no facade includes or transitive dependencies.                                                           |
| B9  | Separate new public example; install and build consumers from installed headers/targets only, linking DAL::cpp and DAL::public respectively. No source-tree include leak.                                                                                                                     |
| B10 | dal_check_generated after Machinist generation, no drift; Windows/MSVC XLL build, actual formulas/registration/help and COMPLETE/PARTIAL results. Missing Windows/Excel is pending evidence, not portable PASS.                                                                               |

B6 deterministic work fixture: D0/D1, maturity-only Bermudan PUT S=100,
sigma=.2,r=.05,q=.03,N=41,upper=400,steps=2,theta=1,rannacher=0;
all risks on, Q bytes, valuation work cap large. N-2=39, each scenario does
two solves →78 work. M=17; full work=1326. Cap 1326 completes; cap 1325
stops the last scenario before its second solve, consumes1287, retains
previously completed risks, aggregate WORK_LIMIT. Cap 78 finishes base and
marks every bump NOT_RUN_WORK_LIMIT, price readable. Cap 77 attempts one
base solve then stops, aggregate BASE_FAILED. These status tests do not
require coarse-grid risk stability or numerical qualification.

Set cap 390 (base + four spot scenarios) to check Delta/Gamma retain their
computed status when later axes stop. Compare each completed risk against
the unlimited-work run, including unavailable stability status if present.
For per-scenario limits with global large, valuation cap77 makes base fail
with SCENARIO source. Use the private seam to spend one extra solve on
Rate[0] when testing a per-bump cap with successful base; keep separate from
product numeric qualification. With sigma=0,r=q=0,American PUT,N=41,
upper=400,steps=2, risk delta-only and global/valuation work=0, the already
admitted intrinsic layer needs no solve: price success, PARTIAL degenerate
risk, work=0. Do not hardcode zero work as a preflight failure.

For scenario caps use N1 full Q (17), N4 full Q (25), and delta+gamma-only
Q (5), at M and M-1; caps count base. At sigma=0 cap1 admits base, cap0
preflight rejects. For bytes first obtain deterministic envelopes P/W/Z on
the same build with generous fixed Q caps. Re-evaluate with each cap equal
to P or W, then one byte less separately; equal admits, smaller rejects
before rollback; plan/records remain equal except budget snapshots. Z is
the simultaneous-live planned total and must bound the observed peak.
Test size_t overflow via maximum count/cap values and maximum legal N/step
layout with checked arithmetic before any allocation. Maximum legal layouts
need not overflow 64-bit size_t: use cap0 to reject their envelopes, and
directly check private checked-add/multiply helpers with SIZE_MAX+1 and
SIZE_MAX*2 operands. Do not allocate huge buffers merely to provoke bad_alloc.
Python/Excel reject unsigned overflow
at conversion. Repeat with a smaller per-scenario byte/step cap.

## Frozen section 11 performance contract

Full-risk axis count d=2+2m, rollbacks M=1+4d=9+8m: one segment17,
two segments25. Delta/Gamma share spot work. Disabled/rejected axes reduce
the actual count. Getters perform zero rollback. Retained memory is full
base history plus O(M+d) summaries and plan; American base region envelope
can be O(N*Nt). Peak additionally holds one full bump and scratch. No O(N)
whole-interface memory claim and no AAD one-reverse complexity claim.

P1 existing prices: select the current `pde_perf --early-exercise` Bermudan,
American and NearZero cases exactly as source defines them; Daily also runs
because this executable exposes no per-case selector. This is still one
small driver, not a full benchmark matrix. All use D price defaults except
NearZero (S=80,sigma=1e-8,r=-.2,q=-.1,American,theta=.5,ordinary_steps=1,
rannacher=0). Preserve price/status/work semantics. Paired master/head
Release comparison, two rounds of at least ten interleaved process samples
per side, alternating first side, per-round best-of-ten minimum. A persistent
>4% regression in both rounds must be repaired or explicitly accepted by
the owner. Unstable/noisy samples are INCONCLUSIVE. No local S0 timing.

P2 new risk cases use D settings with explicit upper=400 and Q byte/work
caps solely to avoid unrelated admission failures: one-segment Bermudan PUT
S=100 with [C,D1],r=.05,q=0 (17); same American (17); N4 noncoincident PUT
S=100 (25); N10a boundary midpoint with D N=401,h=2dx (PARTIAL case).
Do not mix high-resolution Q meshes into throughput acceptance.

Reference for P2 is an independent driver loop over the exact same admitted
scenarios and already fixed common plan, calling the private rollback and
extracting the same diagnostics; it does not call the risk evaluator or reuse
its difference/status assembly. It includes matching input copies, planning,
ownership and diagnostics to avoid an empty/no-diagnostics baseline. Verify
plan/price/status/solve/work identity before timing. Run two rounds of ≥10
interleaved process samples, per-side minimum, calibrated native repetitions
identical for the two sides. In each round, `100*(risk_min/reference_min-1)`
must be ≤15%. Report both rounds; a missing/failed case is not a pass. The
driver must expose case selection for focused repair. New cases remain
separate from the existing formal benchmark allowlist.

P3 projection: one pre-evaluated P2 Bermudan17 and two-segment25 result;
repeat Risks/Scenarios getter pairs R=1,10,100,1000, dropping each detached
copy before the next. Compare compute+copy with native compute plus identical
explicit copying. All native solve/work counters fixed, retained native
payload constant, no retained per-read history. Record binding incremental
time, output bytes and peak detached allocation. Linear complexity means
exact record/cell counts proportional to R and output size, no hidden
valuation; no invented millisecond or timing-ratio gate is added for bindings.

P4 conditional shared-theta coverage: if actual changes reach the common
theta implementation, include the existing European CN `pde_perf` allowlist
case with `--samples 10 --confirmation-rounds 2 --threshold-percent 4`.
If not touched, record it as excluded. Banded/tape cases require actual
affected callers; passive PDE risk alone is not a reason to run them.

All timing evidence includes isolated base/head source/build directories,
immutable SHAs, dependency SHAs, compiler/CMake/flags/CPU/ISA/AAD backend,
DAL_NUM_THREADS=4 on both sides, actual thread configuration, fresh build-tree
binary hashes before/after, raw samples, minima per round, numerical counters
and exclusions. Native envelope/capacity data and process RSS have distinct
labels. No stale staged binary, single sample, changed count/threshold or
historical European verdict can clear this gate. Follow the
[benchmark workflow](../../references/benchmark-workflow.md).

## Stage and release handoff

S0 delivers only these controls, API decision and artifact index, with content
checks and a documentation PR. S1/S2 select old-price/planner and core
correctness/resource cases; S3 completes N1–N12 independent qualifications;
S4 completes B1–B7/B10 projection checks; S5 completes performance, B8/B9,
published method/examples and CHANGELOG. Reuse applicable exact-source evidence.
Each product-code PR needs independent focused review and resolution of
blocking findings. Preserve component boundaries and standalone core examples.

Only after all mandatory smooth numerical risks meet the frozen gates,
non-smooth cases are correctly rejected, binding/lifetime/resource/build
controls are met, Windows evidence exists and selected performance gates
are resolved may the full first-version feature be accepted. Unavailable
environments or insufficient references stay explicit pending items.
