# F03 fixed-grid financial acceptance

Status: active implementation after accepted native sampled-step PR #505,
merged as `72cac784020c106a3e748d76441540e538cfc973`.
Controlling source: original detailed plan H.3, three-layer PDE acceptance.

## Remaining requirement and scope

#504 accepts the numeric discrete step and independent multi-step differences;
#505 adds native activity, events and independent serial composition. H.3 still
requires final financial price/Greek validation with grid convergence separated
from discrete derivative correctness. Complete that boundary before P04.
Implicit stationary-fit H.4 already has accepted numeric/native references,
including nonzero residual and the complete residual-Hessian Jacobian; do not
repeat that work or introduce an unrelated optimizer.

Use existing public sampled-step interfaces with a fixed physical S-grid and
fixed time schedule for Black-Scholes call/put rollback. This increment validates
composition and provides a usable example; it does not modify the accepted
numerical kernel, provider/mesh derivatives, American projection or adaptivity.

## Numerical and activity contract

1. Keep physical grid, evaluation location, expiry, dividend yield and time
   schedule passive. Choose a grid with S0 exactly on a node at all refinement
   levels. Provide separate explicit parameter inputs r, sigma and strike.
2. Model coefficients are rate r, drift (r-q)*S, variance sigma*sigma*S*S.
   Form them through ordinary native expressions before the step chain;
   coefficient vectors share the relevant parameter roots. Their prefix survives
   every step; variance follows the physical volatility chain factor.
   Complete active matrices need valid recording slots even for constant zero
   cells. Form a recorded `0.0 * strike` expression and reuse its handle for zero
   terminal/boundary entries; do not pass detached `Number_(0)` cells.
3. Terminal call/put states are max(S-K,0) and max(K-S,0) respectively. Strike
   affects both terminal payoffs and external boundaries. Freeze the max branch
   at each parameter point, avoid strike-on-node equality in the derivative
   fixture, and require every bumped side to rebuild the complete request.
4. At time-to-expiry tau, call boundaries are 0 and
   Smax*exp(-q*tau)-K*exp(-r*tau); put boundaries are K*exp(-r*tau) and 0.
   Both sides are explicitly external. Native boundaries retain rate/strike
   expressions at the new step time. Never omit discounted RHS risk.
5. Pass each native solution as the next active old-state matrix. The independent
   numeric reference feeds detached solutions into its next dense finite-difference
   time step, using no DAL cache/stencil/solve implementation.
   Two layers represent call/put; channel objectives and layers remain distinct.
6. Use a declared damped initial schedule: four half-size implicit steps cover
   the first two ordinary time steps, then Crank-Nicolson for the remainder.
   The complete schedule sums exactly to expiry and remains the same for all
   bumps, modes and compared implementations. Do not add smoothing silently.

## Independent acceptance

7. Test the full chain's primal and r/sigma/K risks on a small fixed mesh against
   independent scalar/dense discrete execution and complete centered finite
   differences at three declared step sizes. Differences must rebuild terminal
   state, coefficients and boundaries at every parameter point. Compare each
   call/put and a weighted objective with direct parameter terms.
8. Validate scalar and bounded vector channels on the same two-layer request,
   repeated sweeps and a zero lane, checking actual reports and shared parameter
   accumulation. Reuse #505 lifecycle/resource proof; add no duplicate matrix.
9. For three predeclared grid/time refinements, compare prices, rho, vega and
   strike derivative to independent closed forms using erfc normal probabilities.
   State absolute model/discretization tolerances separately from tight discrete
   derivative/finite-difference tolerances. Report all errors and refinement
   behavior; do not adjust method/tolerances after a failure without a recorded
   mathematical reason and retained original evidence.
10. Check call-put parity and derivative parity with their discretization error
    budget. Finite-domain boundaries are approximations; choose a declared
    sufficient domain and disclose that limit rather than treating Black's
    continuum value as an exact fixed-grid oracle.
11. Keep a tiny regression for boundary/terminal contributions with nonzero
    rate, volatility and strike risks. A zero-volatility special case alone is
    inadequate to accept the financial integration.

## Delivery and performance boundary

12. New cases use a named native PDE suite already selected by six sanitizer
    filters. Run only new financial cases and any genuinely changed old helper
    callers locally. Verify installed compilation if a new example uses the
    public API. Publish methodology/example and qualifying changelog changes
    according to the final scope.
13. No existing production object/header changes are intended. Prove old
    archive/caller identity and retain accepted timing. Informational timing
    selects only small/medium complete financial requests and counts setup,
    every step's capture, reverse, reports and release; no full benchmark matrix.
14. Review all CI/Codacy/review findings on the new PR; verify actual named-case
    execution in fourteen profiles, guarded merge and accepted/merged tree
    equality. Only then close F03 and begin P04 in another PR.

## Frozen fixture and convergence admission

The initial K=113, grids 41/81/161 proposal is retained in session evidence.
Independent execution shows changing payoff-cell phase makes price/strike errors
nonmonotonic. Before any DAL financial test execution, replace that proposal with
S0=100, K=110, r=.05, q=.02, sigma=.20, T=1 and Smax=400; grids 21/61/181
and ordinary time counts 40/120/360. Refinement by three preserves K at each
payoff-cell midpoint and S0 at a physical node. This changes fixture sampling,
not the numerical method, payoff or derivative contract.

The independent banded solver uses direct uniform-grid Black-Scholes coefficients,
not DAL difference/generator/cache code. Complex-step derivatives with step `1e-25`
freeze the full discrete r/sigma/K risks; real-strike payoff branches are fixed.
All coefficients, terminal states and new-time boundaries are rebuilt. These
values are a second oracle in addition to the required independent centered
differences; no complex-step or SciPy dependency enters shipped tests.

Predeclare these absolute continuum error ceilings for this fixture, with h the
physical grid spacing: price `0.0002*h*h`, rho `0.002*h*h`, vega `0.006*h*h` and
strike derivative `0.00012*h*h`. Require each absolute error after refinement to be at most
one quarter of the preceding error, for both layers and all four quantities.
These are executable fixture acceptance ceilings, not universal PDE error bounds.
They accommodate the independently observed coarse-grid truncation and approach
second-order behavior without equating continuum and discrete derivatives.

Freeze tight independent-discrete tolerances separately: prices `1e-9` and risks `1e-8`
on the three refinement fixtures. The small independent-dense fixture has n = 9,
ordinarySteps = 8, with the same physical domain, parameters and damping schedule.
Full centered differences use absolute perturbations `1e-3`, `5e-4`, `2.5e-4` for
rate/sigma and strike bumps `0.1`, `0.05`, `0.025`, remaining inside the original payoff cells.
Require r error <= `100*bump*bump+1e-7`, sigma error <= `500*bump*bump+1e-7` and
strike error <= `1e-7`. Retain all three sizes and the weighted objective
`1.25*call - 0.75*put + 2*r - 3*sigma + 0.01*K`; weighted-difference ceilings combine the
two layer ceilings using the absolute weights. Discrete frozen-reference
acceptance remains tighter than these truncation-aware difference checks.

Call-put continuum parity is S0*exp(-q*T)-K*exp(-r*T); r parity is
K*T*exp(-r*T), sigma parity zero and K parity -exp(-r*T). Apply separately
declared discrete time-error allowances, rather than expecting exact continuum
discounting from theta time integration. Retain complete reference rows and
observed parity error in the acceptance report. On the three refinement grids,
with ordinary dt=1/ordinarySteps, freeze price-parity ceiling `0.2*dt*dt`,
rho-parity ceiling `6*dt*dt`, vega-parity ceiling `1e-8` and strike-parity
ceiling `0.002*dt*dt`. These checks apply to the declared sufficiently wide domain,
not the small n = 9 mesh whose coarse finite-domain boundary error is larger.

## Implementation and bounded acceptance set

Implement a small example/test-support caller using the accepted public numeric
and native APIs. The native recording helper accepts the caller's recording
scope and registered r/sigma/K inputs, records both option layers, and returns
prices plus step event handles. It does not own/reset the caller's tape or hide
mode changes. A self-contained new european_aad_fd example owns its mode/scope,
prints prices/Greeks, and closes before leaving the mode. Do not modify existing
production kernels or retrofit the old ThetaScheme example with changed behavior.

Keep the independent dense reference in a separate test helper: direct uniform
generator plus full matrix elimination, no DAL stencil/solve/cache calls. All
bumps run that complete independent chain. Frozen literals carry SciPy reference
values without introducing Python dependencies in C++ tests.

Use six named financial cases: independent dense primal/full differences;
frozen small-mesh Greeks; scalar/vector-4 weighted and direct-root lanes with
repeated sweeps and zero lane; one combined three-level frozen-reference,
continuum-convergence and price/risk-parity case; explicit nonzero terminal and
boundary dependency controls; and the minimum four-half-step damped schedule.
Combining the three refinement assertions avoids nine complete chains across
three tests. Scalar/mode-specific checks reuse the tiny fixture, not the full
refinement grid matrix. Convergence runs only three complete width-2 chains once.
Every actual step report must have two layer rows and the requested channel
count, with physical forward/transpose errors within the declared policy.

Freeze dependency-control minimum omitted-risk magnitudes from the independent
small-mesh calculation before native execution: terminal-strike omission at
bump `0.025` must exceed `0.05` (call) / `0.5` (put); boundary-strike omission at bump `0.025`
must exceed `1e-6` / `5e-5`; boundary-rate omission at bump `0.00025` must exceed `5e-5` / `0.003`.
The complete derivative still must satisfy its original centered-difference
ceiling. The minimum n = 5 / ordinarySteps = 2 fixture uses four implicit half steps,
with independently frozen price/Greek values and both layer reports.

The immutable #505 archive/caller proof is reusable when every production source
and relevant build input remains identical. This increment need not relink or
retime legacy callers merely because tests/examples/docs were added. Compile/run
the new example and six financial cases, then strict-check only their new units
in OFF/combined diagnostic ON. Remote required CI remains mandatory.
