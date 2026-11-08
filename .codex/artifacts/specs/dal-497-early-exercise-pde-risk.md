# DAL-497 early-exercise PDE risk contract

Status: active S0 contract, schema version 1. Controls S1–S5 of DAL-497.
Core fixed-plan extraction and owning repricing risk are implemented. The
public-facade/Python/Excel risk projections and full independent numerical
qualification remain pending. This contract establishes requirements, not
qualification evidence; product tests, builds and benchmarks were not part of S0.

Authority: accepted P3 plan, attachment `01a1b747-5bcd-777f-ba24-cbcabee04578`,
and execution authorization in comment `01a1b749-b0bc-7cea-aa9f-ca21159a276d`.
The [API decision](../api-notes/dal-497-early-exercise-pde-risk.md) fixes exact
names and projections; the [acceptance contract](dal-497-pde-risk-acceptance.md)
fixes fixtures, profiles and gates. These three files supersede the plan's
provisional choices. Changes require an explicit contract decision before
implementation, never retrospective threshold changes to clear a failure.

## Baseline and scope

Read-source baseline: master `4e9eb5eb7cae770826e6f4e0a05aeb9bf4c4c44e`.
Relative to the plan's `fdecebc8d69764f81ee71b6c8354b85581193730`, the only
additional commit is #681, Python benchmark retry support. The early-exercise
core, public price header, Python/Excel price bindings and PDE methodology
are unchanged. Recheck latest master and direct dependencies at each stage.

Source contracts examined:

- `dal-cpp/dal/math/pde/earlyexercisepde.hpp` and its implementation: owning
  passive price result, date events, original LCP residuals, monotonic planner,
  work accounting and storage envelopes.
- `dal-public/src/earlyexercisepde.hpp`: aliases rather than a second solver.
- `dal-public/src/europeanpderisk.hpp` and its implementation: a different
  AAD contract that requires an empty native tape and changes recording mode.
- `dal-python/src/bindings/earlyexercisepde.cpp`,
  `dal-excel/src/__earlyexercisepde.hpp` and its implementation: strict inputs,
  detached getters, GIL release and atomic owning handles.
- `docs/methodology/pde/early-exercise.md`, existing core/public tests,
  `dal-cpp/benchmarks/pde_perf/pde_perf.cpp` and component-owned examples.

Existing #602 evidence is historical, not qualification for this feature.
The [published method guide](../../../docs/methodology/pde/early-exercise.md#owning-core-risk-request)
describes the implemented core risk surface and distinguishes pending
projections and qualification from supported behavior.

Scope is one-dimensional BS vanilla CALL/PUT, constant decimal volatility,
piecewise deterministic continuously compounded rate/dividend yield,
explicit evaluation/maturity dates and ACT/365 calendar-day time. Retain the
existing dated Bermudan projection and American theta/penalty methods.
Deliver Delta, finite-scale Gamma, Vega and individual carry-parameter partials.
No Theta, strike risk, mixed Greeks, quote DV01, calibration chain, boundary
Greek, one-sided fallback, new time scheme or AAD implementation is admitted.

## R1 — owning input and resolved configuration

`EarlyExercisePdeRiskRequest_` owns `valuation_` and `risk_`. Copy before native
evaluation. `Request()` preserves the original values, including an unset upper
and empty default bump vectors. `Plan()` preserves their resolved counterparts.
No global evaluation-date lookup or source-handle callback is permitted.

The valuation obeys the existing price-request constraints. Carry end dates
strictly increase after evaluation and finish exactly at maturity. Bermudan
dates strictly increase in [evaluation, maturity] and include maturity;
American dates are empty. No sorting, date rounding or carry coalescing.

`ValidateEarlyExercisePdeRiskSettings` validates settings locally;
`ValidateEarlyExercisePdeRiskRequest` also validates the valuation and
request-dependent lengths/spacing. Neither rolls back or qualifies a Greek.
All fields are validated even if their corresponding risk is disabled.

## R2 — axes, values and units

Requested record order is `Delta, Gamma, Vega, Rate[0], DividendYield[0], ...,
Rate[m-1], DividendYield[m-1]`, filtered by the four switches without reordering.
All off is invalid. Carry risk selects both parameter families for every segment.
There are `3+2m` records when all switches are on. Record and scenario indices
are zero based and dense after filtering; derivative order is 1 or 2.

| RiskId             | ParameterId        | Order | Unit                                |
|--------------------|--------------------|-------|-------------------------------------|
| `Delta`            | `Spot`             | 1     | `price per spot price unit`         |
| `Gamma`            | `Spot`             | 2     | `price per squared spot price unit` |
| `Vega`             | `Volatility`       | 1     | `price per decimal volatility`      |
| `Rate[j]`          | `Rate[j]`          | 1     | `price per decimal rate`            |
| `DividendYield[j]` | `DividendYield[j]` | 1     | `price per decimal dividend yield`  |

Bracketed indices use unsigned decimal ASCII with no padding. Segment fields
are absent for Spot/Volatility; carry records contain j, preceding end date
(evaluation for j=0), and end date. `parameterValue_` is the base scalar value.
Spot/strike/price are quote units per unit underlying; no notional conversion.
Vega ×0.01 is an explicitly scaled vol-point exposure. Rate ×1e-4 is that
carry parameter's first-order 1bp P&L, never quote DV01.

## R3 — absolute bumps and validation

The API decision fixes settings spelling/defaults. Resolve base upper once to
`4*max(S,K)` when absent; `dx=upper/(N-1)`. Spot fine bump h is default `2dx`,
coarse `2h`. Volatility/rate/yield configured bump b is coarse b, fine b/2.
Empty carry bump vectors mean m copies of 1e-4; otherwise length is exactly m.
No silent resizing, bump reduction, domain expansion or budget increase.

All supplied bumps, derived halves/doubles and parameter differences must be
finite, positive and representable as distinct binary64 parameter values at
both scales. A collapsed floating-point offset is an invalid request, not zero
risk. For a domain-inadmissible stencil, check distinctness only for its
in-domain parameter points. Explicit spot h must satisfy h/dx = integer k≥2:
accept when `abs(h/dx-round(h/dx)) <= 8*epsilon64*max(1,abs(h/dx))`,
with rounded ratio at most 2^53-1. Preserve supplied h, never snap it to k*dx.
This tolerance only handles binary64 arithmetic, not a user-selected fraction
of a cell. Check arithmetic overflow before allocating or constructing offsets.

## R4 — domain and degenerate diffusion

Spot requires the full coarse stencil `S-2h>0` and `S+2h<upper`.
Volatility requires `sigma-b>=0`. A valid input whose stencil fails these
conditions retains a risk row with `UNAVAILABLE_DOMAIN_BOUNDARY`; no invalid
scenario is created. Other axes can proceed. At sigma=0 every requested risk
is `UNAVAILABLE_DEGENERATE_DIFFUSION`, with only the base scenario planned.
Small positive sigma has no automatic zero-risk or success exemption.

## R5 — one numerical plan

Build the legal requested scenario set before planning. Base is always present.
The spatial grid is exactly the existing uniform price grid using resolved
base upper, shared by every scenario, without spot-dependent rebuilding.
Use the unchanged event anchors and base/damping partition. For each partition,
double its subdivision count until every legal scenario satisfies both current
price monotonicity inequalities on every actual substep. Taking the union of
rounded time arrays is insufficient. Keep exact event dates, optional dates
for artificial nodes, theta, carry index and Rannacher semantics.

Use one price-unit epsilon for all rollbacks:
`min_s(absoluteTolerance + relativeTolerance*max(S_s,K))` over admitted
parameter scenarios including base. Normalize each scenario's diagnostic
residual by its own max(S_s,K); use K for risk stability/acceptance metrics.
Penalty, linear-error limit and iteration cap are identical across scenarios.
No replanning after a solver or work failure. The private planner/rollback
split must leave price-only single-request planning and epsilon unchanged.

`planId_` identifies canonical numeric plan content, not request identity or
an address. Canonical version-1 content comprises schema, resolved upper,
spatial/time nodes and event flags, step fields, common epsilon, theta,
effective damping, penalty, iteration cap, linear limit and interpolation
policy. Serialize integers in little endian fixed widths (indices/counts u64,
schema u32, booleans u8), doubles as IEEE-754 binary64 little endian (normalize
-0 to +0), dates as signed i64 Excel serials with a u8
presence tag, strings as u64 byte length plus UTF-8. Field order follows the
API Plan record; vectors carry a u64 count. `planId_` is `p3v1:sha256:` followed
by the 64 lowercase hex characters of SHA-256 of these bytes. It is a diagnostic
content fingerprint; compare actual arrays when testing plan equality rather
than treating a hash as proof. Do not include the ID itself, budgets, requested
bumps, origin tags or execution counters. Charge canonicalization scratch and
retained ID storage to their respective budgets. No cross-version identity guarantee.

## R6 — complete and deterministic scenario execution

Execute sequentially: base, Spot, Volatility, then Rate[0], DividendYield[0],
and so on. Within an axis the offset order is coarse minus, coarse plus,
fine minus, fine plus. Spot is selected by either Delta or Gamma and shared
by both. Scenario IDs are `Base`, then `<ParameterId>:C-`, `:C+`, `:F-`, `:F+`.
Deduplicate only identical complete numeric parameter vectors; keep the first
ID and reference its index from every stencil. Omit axes rejected by R4.

Rebuild terminal payoff, boundary envelope, operator, upwind choices, American
penalty iterations/Bermudan projection and contact diagnostics for each
scenario. Never reuse the base exercise mask. Future exercise-set or upwind
changes are diagnostics, not automatic unavailability. Scenario prices are
published only after a complete successful rollback. Keep the base full
owning result; reduce each bump result to a summary and release its full
history before starting the next. Getters never compute.

## R7 — finite-scale estimates and recorded stencils

For spot scale a:

```text
Delta(a) = (V(S+a)-V(S-a))/(2a)
Gamma(a) = (V(S+a)-2V(S)+V(S-a))/(a*a)
fine a=h; coarse a=2h
```

For vol/rate/yield scale a, the first derivative uses the same central formula
with fine a=b/2 and coarse a=b. Return the fine estimate only after R8/R9
admission. No extrapolation. Each risk retains ordered `coarseStencil_` and
`fineStencil_` entries `(scenarioIndex_, weight_)`: minus, base if second
derivative, plus. Weights are respectively (-1/(2a), +1/(2a)) or
(1/a², -2/a², 1/a²). A stencil rejected at R4 has empty entries.
Unavailable values never become zero, NaN sentinels or cached previous values.

Always tag `Method()=BlackScholesEarlyExerciseFullRepriceFixedPlan`,
record method `FiniteScaleCentralDifference`,
basis `NATIVE_MARKET_PARAMETER_PARTIAL`, `aadUsed_=false`.
Gamma interpretation is `FINITE_SCALE_MODEL_APPROXIMATION`; first derivatives
use `FINITE_SCALE_FIRST_DERIVATIVE`. Gamma spans multiple cells and is not the
classical second derivative of the piecewise-linear price interpolant.
Continuous-model accuracy additionally requires the independent N qualification.

## R8 — availability and non-smoothness

Define signed `stepDifference_=fineEstimate_-coarseEstimate_`.
Its absolute normalized value is compared with settings: Delta directly,
Gamma ×K, other first derivatives /K. Equality to tolerance passes. Defaults
are 2e-3, 2e-2, 3e-3 for Vega, 3e-3 for rate and 3e-3 for yield.
This is a stability screen, not a proven error bound.

At evaluation exercise, extract every positive-intrinsic contact run from base
and that risk's stencil scenarios. An unclipped edge gives the closed adjacent
grid-node bracket. Intersecting any such bracket with the closed interval
[S-2h,S+2h] makes Delta/Gamma unavailable near an exercise boundary, including
endpoint equality. A clipped edge has no fictitious exterior bracket; R4
rejects a stencil reaching the domain edge. Membership is EXERCISE only when
both interpolation nodes (one if exactly on a node) belong to the same contact
run; mixed membership is BOUNDARY, otherwise CONTINUATION. With no evaluation
exercise level use NOT_EXERCISABLE, not a missing diagnostic.

For vol/rate/yield, evaluation BOUNDARY or a change of evaluation membership
between base and any stencil scenario yields `UNAVAILABLE_EXERCISE_SWITCH`.
For spot, a non-boundary full stencil entirely in the same positive intrinsic
region may publish `AVAILABLE_EXERCISE_REGION`. For parameter axes this tag
requires all evaluation memberships EXERCISE. Other successful estimates are
`AVAILABLE`. Zero-payoff contact is excluded by the existing price definition.
Report evaluation membership, future contact change and upwind change
independently. Never differentiate region locations or smooth the obstacle.

## R9 — status, failure and errors

Exact enums and fields are in the API decision. Risk reason precedence is:
base failed; required scenario blocked by global work; required numerical
scenario failure; degenerate diffusion; domain boundary; evaluation boundary;
exercise switch; nonfinite difference; unstable scales; available.
Domain/degenerate preclassification still loses to base failure. Preserve
computed finite estimates for diagnosis even when value is unavailable.

Aggregate precedence: BASE_FAILED if base failed for any cause (including its
work cap); otherwise WORK_LIMIT if the global remaining work blocked execution;
otherwise COMPLETE if every requested risk is available; otherwise PARTIAL.
`IsComplete()` is exactly status==COMPLETE. `PriceAvailable()` means base
success only. Failed base diagnostics remain readable through `BaseResult()`.

Scenario status is SUCCESS, NUMERICAL_FAILURE, NOT_RUN_WORK_LIMIT or
NOT_RUN_BASE_FAILED; PLANNED is internal only. A bump stopped before any solve
or completed step is NOT_RUN_WORK_LIMIT, with no solver observation. An
attempted scenario stopped later by global work has NUMERICAL_FAILURE,
solverStatus WORK_LIMIT and `workLimitSource_=GLOBAL`; later scenarios have
NOT_RUN_WORK_LIMIT. Base always attempts execution; it can succeed with zero
solves or fail with solverStatus WORK_LIMIT. Base failure leaves every bump
NOT_RUN_BASE_FAILED. Per-scenario work exhaustion
uses source SCENARIO, invalidates its dependent risks and allows independent
scenarios to continue. A global stop preserves independent risks already
completed. No returned row may remain PLANNED.

Attempted count includes SUCCESS and NUMERICAL_FAILURE only; not-run count is
the two NOT_RUN statuses. Successful + failed + not-run equals planned;
attempted equals successful + failed. Overall work-limit source/index records
the first actual cap stop, including a not-run bump; NONE/absent if no stop.
Every returned not-run bump has zero observed work/steps/capacity. An admitted
zero-solve successful scenario is attempted, not skipped.
NOT_RUN_WORK_LIMIT rows have GLOBAL source and no solverStatus/failure;
NOT_RUN_BASE_FAILED rows have NONE source and no solverStatus/failure.
A nonfinite interpolated price converts the risk-path scenario to numerical
failure NONFINITE before any price or dependent estimate can be published.

Error identifiers are string prefixes in DAL `Exception_`, not new exception
classes. Python type/keyword errors use TypeError, semantic/range input errors
ValueError and evaluation/getter failures RuntimeError; preserve the identifier
and field constraint in all cases. Excel uses existing Error text handling.

| Identifier                             | Trigger and required context                                        |
|----------------------------------------|---------------------------------------------------------------------|
| `InvalidEarlyExercisePdeRiskRequest`   | Input; field path, offending value/type and legal constraint        |
| `EarlyExercisePdeRiskBudgetExceeded`   | Preflight; budget field, required, limit and overflow if applicable |
| `EarlyExercisePdeRiskValueUnavailable` | Scalar risk getter; riskId, status and reason                       |
| `EarlyExercisePdeRiskUnknownRiskId`    | Unknown or unrequested scalar riskId; requested IDs                 |
| `EarlyExercisePdeRiskPriceUnavailable` | Scalar price getter; aggregate and base solver status               |

Within the new risk entry points wrap invalid valuation/price-budget errors
with the new prefix, preserving their old identifier and offending valuation
field as details. Existing price entry points keep their old prefixes.
Canonical risk IDs must match byte-for-byte, including case, even though DAL
String_ comparisons are normally case insensitive. Diagnostic getter success
does not imply price/risk success. Reason text is explanatory, not a status
parser; stable enums/identifiers carry machine meaning.

## R10 — cumulative resource contract

Frozen defaults: maxScenarios 129; maxWorkUnits 100000000;
numericPayloadBudgetBytes 67108864; workspaceBudgetBytes 268435456.
Caps are nonnegative representable size_t values; scenario cap may be zero
(then preflight rejects the mandatory base). Zero byte caps preflight reject
nonempty layouts. Zero work is valid and only fails if a solve is needed.
No budget resets per bump. The valuation's per-scenario work, step, result
payload and scratch limits also apply to the common plan and every rollback.

Preflight count/size arithmetic is checked for overflow. Count actual unique
legal scenarios, not potential rejected axes. Price-envelope reservation uses
the common actual step count, including all damping/subdivision. Before first
rollback admit all retained capacities, current full bump result, scratch and
planning/copy storage. No optimistic reservation based on observed contact runs.
American region worst case remains ceil(N/2) per nonterminal level.

Charge every owned buffer once in its live category: copied requests, resolved
configuration, spatial/time plan, ID and generated strings, risk/stencil
vectors, scenario/failure summaries, full base and any transient full bump.
Use capacity, not size, including string terminators; string storage within
an object is not counted twice. Bounded diagnostic reason strings have at most
1024 UTF-8 bytes plus terminator (truncate explanatory text at a code-point
boundary; never truncate status, numeric context fields or an error identifier).
No unbounded concatenation outside preflight. The plan ID's length is computed
from canonical bytes before allocation. Implementation may choose a more
conservative envelope; it must report that exact envelope and admit caps before
rollback. Exact-byte tests use that same reported envelope on the same build.

Retained payload includes the base, plan and all summaries. Temporary workspace
includes planner buffers, one full bump and its scratch, and transient copies;
exclude retained storage already charged. Track actual simultaneously live
retained+temporary peak separately. Do not add unrelated phase maxima and call
it an observed simultaneous peak. Envelopes and actual capacity counters exclude
allocator metadata, process RSS, caller memory and detached Python/Excel copies.

Each attempted tridiagonal solve charges N-2 before execution, including failed
solves and every American penalty iteration. Check both global and scenario
remaining budgets before each solve; GLOBAL wins a simultaneous cap hit.
Consumed work is the sum of scenario work; no overshoot, reset or refunded
failed work. Planning itself consumes zero solve-work but its time and memory
count in performance/resource evidence. Native memory allocation failure is
not a successfully evaluated diagnostic result.

## R11 — lifetime, binding and compatibility

The C++ result owns passive binary64 data and supports value copy/move.
const references live only as long as their result. Python readonly properties
and nested/container getters detach copies; copy/deepcopy never evaluate.
Copy inputs under GIL, release GIL only for pure native evaluation, reacquire
before Python object construction. Excel constructs local complete values then
atomically replaces output handles; archive serialization is unsupported.

Never touch tape, recording mode, adjoint channel count, seeds, adjoints,
reverse events or cleanup capacity. Existing recording and finished graphs
are allowed. Subsequent caller Reverse must behave exactly as its control.
No AAD::Number_ in owned data. Independent concurrent calls share no mutable
plan, region, budget or result state.

Compatibility requirements are additive, with these real constraints:

- Preserve every existing price field/default/name/status/method/date unit and
  failure getter. Single-request planner refactoring must reproduce price,
  grid, time, steps, regions and work for the same old request.
- Risk base may need a finer common plan. When plans coincide require equal
  results; otherwise expose the difference and apply the frozen numerical
  tolerances. Never assert risk base is bitwise identical to old price-only
  evaluation with a different plan.
- European native AAD's empty-tape and cleanup policy is incompatible with
  this passive evaluator; do not copy it. Keep its API unchanged.
- An old price result cannot be converted to a risk result without evaluating
  the copied request. There is no hidden risk computation on an old getter.
- Bindings gain new names only; old function arguments/tables do not change.
  Schema strings, not C++ enum ordinals, are interchange identifiers. New C++
  types require rebuilding as usual; no ABI/pickle/Excel archive promise.

## Stage ownership and completion

| Requirement | Primary acceptance controls | API decision surface |
|-------------|-----------------------------|----------------------|
| R1          | B1, B3, B4                  | Settings/request     |
| R2          | N12, B5                     | Risk records/units   |
| R3          | N7, N11, N12                | Settings/origins     |
| R4          | N9, N10                     | Availability         |
| R5          | N4–N8, B5                   | Plan/time/steps      |
| R6          | N3, N9, N12, B7             | Scenarios            |
| R7          | N1–N3, N7, N11              | Records/stencils     |
| R8          | N5, N10, N11                | Membership/status    |
| R9          | N9, B4–B7                   | Status/errors        |
| R10         | B6, P1–P3                   | Plan/execution       |
| R11         | B1–B5, B8–B10               | Owning result        |

S1 isolates private planning/rollback while preserving old price behavior.
S2 implements this core owning contract. S3 supplies independent numerical
qualification. S4 implements public/Python/Excel projections after schema
stability and releases after S3 merge. S5 supplies scoped performance, installed
consumers, examples and current-state methodology/CHANGELOG updates.
One stage per PR, coordinated on DAL-497. A numerical failure changes evidence,
not this contract. No automatic expert dispatch or historical execution restart.

Preserve core examples and targets under `dal-cpp/examples/`; any new core
risk example uses only DAL::cpp. Add facade demonstration separately under
`dal-public/examples/early_exercise_pde_risk`, target
`early_exercise_pde_risk_public`, linking DAL::public. Follow the
[component boundary contract](../../references/component-boundaries.md).
