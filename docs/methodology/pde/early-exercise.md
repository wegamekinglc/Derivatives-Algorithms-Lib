# Bermudan and American PDE Pricing

`PDE::EvaluateEarlyExercisePde` prices one-dimensional Black–Scholes vanilla
calls and puts with constant nonnegative volatility and piecewise-constant,
deterministic interest rates and continuous dividend yields. Bermudan exercise
uses dated obstacle projection; American exercise uses a theta/penalty solve.
Requests and results own their passive data. The implementation is in
`dal-cpp/dal/math/pde/earlyexercisepde.hpp` and
`dal-cpp/dal/math/pde/earlyexercisepde.cpp`.

This interface returns `double` prices and diagnostics. It does not provide
Greeks, AAD, local volatility, stochastic carry, discrete cash dividends,
barriers or arbitrary script payoffs. The separate [European risk interface](aad.md)
and [LSMC exercise engine](../monte-carlo/lsm.md) retain their own contracts.

## Dates, Carry and Units

The contract supplies explicit evaluation and maturity dates, positive strike,
`CALL`/`PUT`, and `BERMUDAN`/`AMERICAN`. The market supplies positive spot,
nonnegative decimal volatility and a nonempty carry vector. Each carry row
contains an end date, continuously compounded decimal rate and decimal dividend
yield. End dates strictly increase from after evaluation through maturity;
the last row must end exactly at maturity. A row applies from the preceding
end date, or evaluation for the first row. Negative rates and yields are
allowed when the boundary terms remain representable.

Time is calendar-day difference divided by 365, including across leap years.
There is no business-day adjustment or global evaluation-date lookup. Bermudan
exercise dates must strictly increase within `[evaluation, maturity]` and
include maturity; evaluation-date exercise is allowed. American requests must
have an empty exercise-date list.

Carry knots and exercise dates are exact time-grid events. A step never crosses
a carry knot, and Bermudan projection occurs at the event date after rolling
back to it. Additional subdivision times need not be calendar dates: their
`calendarDate_` is absent, rather than rounded to a day. The time grid is in
ascending time order, with zero-based indices. Prices, spot, strike and value
layers share the underlying's quote units, per unit underlying; no notional or
currency conversion is applied.

## Scheme and Defaults

The solver uses a uniform spot grid on `[0, upper]`. The default upper bound is
`4 * max(spot, strike)`; a supplied upper must exceed both. Interior rows use
central differences where their off-diagonal coefficients are nonnegative,
otherwise a monotone upwind drift stencil. `upwindRows_` reports this switch.
The success price is linearly interpolated from the evaluation-date value layer.

Python and Excel use the snake-case setting names below; C++ uses the
corresponding camel-case members with a trailing underscore.

| Setting                        | Default             | Meaning                                                                                 |
|--------------------------------|---------------------|-----------------------------------------------------------------------------------------|
| `grid_points`                  | 401                 | Total nodes, including both endpoints; range 5–100001                                   |
| `upper`                        | Unset               | Resolve to `4 * max(spot, strike)`                                                      |
| `ordinary_steps`               | 500                 | Positive base interval count before event splitting and refinement                      |
| `theta`                        | 1                   | Fully implicit; only 1 and 0.5 are supported                                            |
| `rannacher_intervals`          | 2                   | CN damping intervals after terminal payoff and Bermudan projections; 0–`ordinary_steps` |
| `penalty_factor`               | 100000000           | American penalty coefficient; range 1–10^12                                             |
| `absolute_tolerance`           | 1e-8                | Positive price-unit residual tolerance                                                  |
| `relative_tolerance`           | 1e-10               | Relative part of residual tolerance; in (0,1]                                           |
| `linear_backward_error_limit`  | 1e-12               | Positive componentwise linear-solve error limit; at most 1                              |
| `max_iterations_per_step`      | 100                 | Positive American iteration limit                                                       |
| `max_actual_steps`             | 100000              | Positive limit including event, damping and stability subdivisions                      |
| `max_work_units`               | 100000000           | Interior-node solve-work limit                                                          |
| `numeric_payload_budget_bytes` | 67108864 (64 MiB)   | Owning native result-capacity limit                                                     |
| `workspace_budget_bytes`       | 268435456 (256 MiB) | Native temporary-workspace limit                                                        |

**The default fully implicit scheme is first order in time.** It applies no
Rannacher splitting; `effectiveRannacherIntervals_` is zero even though the
requested setting is two. Event intervals receive proportional, rounded-up
base step counts, so `ordinary_steps` is not necessarily the actual step count.

Crank–Nicolson is explicit opt-in with `theta=0.5`. Its selected damping
intervals become two implicit half-steps. Both schemes enforce sufficient
monotonicity conditions, including under negative rates; the planner bisects
steps until those conditions hold or a step limit rejects the request. CN can
therefore require many more steps and much more diagnostic storage than fully
implicit stepping. It does not silently switch to fully implicit, disable
diagnostics or relax budgets. CN does not establish global second-order
accuracy for the American free-boundary problem.

At each spatial endpoint the boundary policy uses an envelope of discounted
intrinsic payoffs over future exercise opportunities. American envelopes also
consider eligible stationary points inside carry segments. A finite upper is
still a domain truncation: increasing it and refining space and time separately
are necessary accuracy checks.

## Original LCP Residuals

For an American step, let `A` and `b` denote the boundary-eliminated,
unit-diagonal theta system **without penalty**, and let $g$ be the intrinsic
payoff. The linear complementarity problem is

$$
V-g \geq 0, \qquad w=AV-b \geq 0, \qquad \min(V-g,w)=0.
$$

Penalty iterations are accepted only when the iterate change and all three
interior residuals satisfy the price-unit tolerance

$$
\varepsilon = \mathrm{absoluteTolerance}
  + \mathrm{relativeTolerance}\,\max(S_0,K).
$$

The reported residuals are the maximum obstacle violation
$\max_i(0,g_i-V_i)$, dual violation $\max_i(0,-w_i)$, and complementarity
residual $\max_i|\min(V_i-g_i,w_i)|$. They are measured on that original
unpenalized system, not just on the penalized linear equations. The
`normalized*` fields divide them by $\max(S_0,K)$ for reporting. The separate
linear backward-error limit also applies to every performed solve. Bermudan
steps instead report projection change/residual at exercise dates; American
dual/complementarity fields are absent there.

A small iteration change alone is insufficient. A finite penalty can leave
an LCP residual above tolerance even after its iterates stop moving; increasing
the iteration limit cannot remove that penalty bias. Conversely, residual
acceptance certifies the discrete step, not the error to a continuous-model
price. Grid spacing, time stepping, payoff interpolation, upwinding and finite
domain error remain separate.

## Budgets and Failure

Planning checks the full result and workspace envelopes before rollback,
including all time/step/level records and worst-case exercise-region capacity.
An American request reserves a level for every planned step plus terminal;
each nonterminal level permits up to `ceil(grid_points/2)` disjoint runs.
This reservation can reject a request even if its eventual exercise region
would be small. Execution reports planned envelopes, actual retained capacities
and peak native workspace. Byte budgets exclude allocator bookkeeping, process
RSS, caller storage and detached Python/Excel getter copies.

Each tridiagonal solve consumes `grid_points - 2` work units, including every
penalty iteration. Raising a byte budget does not raise the work or step limits
and does not improve numerical accuracy. Refine the numerical configuration
and then admit its resource requirements explicitly.

Malformed requests raise `InvalidEarlyExercisePdeRequest`. Insufficient planned
storage or step budgets raise `EarlyExercisePdeBudgetExceeded` before returning
a result. Numerical failure during rollback returns a result with a non-success
status and failure diagnostics, including `MAX_ITERATIONS`,
`LCP_RESIDUAL_LIMIT`, `LINEAR_SOLVE_FAILURE`, `NON_MONOTONE_OPERATOR`,
`NONFINITE` or `WORK_LIMIT`.

Only `SUCCESS` publishes a price and evaluation-date values. C++ `Price()` and
`Values()` throw on a failed result; the matching Python properties do likewise.
Excel's Summary has a blank Price and its Grid has blank Value cells on failure;
`GET.PRICE` returns error text. Diagnostic getters remain available. They do
not turn a partial rollback into a valid price.

## Owning Results and Exercise Regions

The result owns the copied request, resolved upper bound, spot/time grids,
evaluation-date values, step diagnostics, execution counts and failure details.
It does not retain an active tape or call back into the source request.
Python container/nested getters and Excel tables are detached copies; reading
them performs no new valuation. Excel handles reject archive serialization.

Time-grid, step and exercise-level records are ordered by increasing target
time. `rollbackStep_` separately records the one-based backward execution order.
Each exercise level identifies a slice of the region array through zero-based
`regionOffset_` and `regionCount_`. Region rows carry inclusive `firstNode_` /
`lastNode_`, a zero-based ordinal within the level, and clipping flags. Follow
the level's slice; the region array is not independently reordered by time.
The terminal level has no early-exercise regions.

Contact requires positive intrinsic payoff and agreement with it within
$\varepsilon$, plus the step's applicable continuation/dual condition. A level
can have zero, one or several disjoint regions. These are grid contact regions,
not an interpolated scalar exercise boundary. For an unclipped run `[a,b]`,
the left boundary is bracketed by `grid[a-1]` and `grid[a]`, and the right by
`grid[b]` and `grid[b+1]`. A clipped edge supplies no outside-grid bracket.
Empty regions do not imply missing diagnostics, and zero-payoff nodes are not
reported as exercise merely because their value is zero.

## C++ Usage

This complete core-only example links `DAL::cpp` and uses explicit dates:

```cpp
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/platform/platform.hpp>
#include <dal/math/pde/earlyexercisepde.hpp>

int main() {
    Dal::PDE::EarlyExercisePdeRequest_ request;
    const Dal::Date_ today(2026, 10, 18), maturity(2027, 10, 18);
    request.contract_ = {today, maturity, 100.0, Dal::OptionType_("PUT"),
                         Dal::EarlyExerciseStyle_("BERMUDAN"),
                         {Dal::Date_(2027, 4, 18), maturity}};
    request.market_ = {100.0, 0.20, {{maturity, 0.05, 0.0}}};
    const auto result = Dal::PDE::EvaluateEarlyExercisePde(request);
    if (!result.IsSuccess()) {
        std::cerr << result.Status().String() << '\n';
        return 1;
    }
    std::cout << std::left << std::setw(12) << "Style" << std::right
              << std::setw(16) << "Price" << '\n'
              << std::string(28, '-') << '\n'
              << std::left << std::setw(12) << "BERMUDAN" << std::right
              << std::setw(16) << std::fixed << std::setprecision(8)
              << result.Price() << '\n';
}
```

For American exercise, set `exerciseStyle_` to `AMERICAN` and clear
`exerciseDates_`. The public facade header
`<dal-public/src/earlyexercisepde.hpp>` re-exports these types and the evaluator
in `Dal`; facade consumers link `DAL::public`. Core examples retain their
existing locations and core dependencies.

See the [Python example](../../python/README.md#bermudan-and-american-pde-pricing)
and [Excel worksheet recipe](../../excel/README.md#bermudan-and-american-pde-pricing)
for the same contract and owning result access.
