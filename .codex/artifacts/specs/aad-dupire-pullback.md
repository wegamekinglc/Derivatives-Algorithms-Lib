# F01: frozen Dupire spread-quote pullback

Status: active requirements for the next Stage B increment. D04 verification
and P01 performance acceptance remain independent gates. This specification
controls the first calibration boundary and its subsequent Hybrid integration;
it does not close the common curve-calibration adapter requirement.

## Source and problem

The [full implementation plan](../plans/aad-implementation.md) requires
`g_quote = g_direct + C_quote^T g_surface` for the actual discrete Dupire program.
`AAD::RiskView_` already interpolates additive implied-volatility spreads, and
`AAD::DupireCalib` already accepts active spreads. Existing public valuation
returns local-vol model-coordinate risks without connecting this calibration
chain. `DupireCalib` transposes its time-major intermediate; Hybrid parameters
are spot-major/time-minor, after each equity component's spot/dividend inputs.
Equal display labels or equal vector lengths cannot certify that mapping.

Relevant sources are `dal-cpp/dal/model/ivs.hpp`, `dupire.hpp`,
`surface/lvmodel.hpp`, `hybrid.hpp`, `hybriddata.hpp`,
`dal-public/src/riskvalue.cpp`, and the existing Dupire/Hybrid tests.

## Function and boundaries

For a fixed base IVS, fixed spot/rate/dividend, fixed quote strike/maturity axes
and fixed calibration grids, define implied volatility as the base value plus
the existing bilinearly interpolated spread matrix. Spread coordinates are raw
absolute decimal volatility: 0.01 is one volatility point. The base IVS's
parameters are fixed and are not added to the differentiated quote axis.

Differentiate the existing central-difference stencil, with relative time and
strike steps 1e-4. Select the stable spot band using the base ATM call, exactly
as the existing calibrator does; spreads do not change the band. Copy boundary
local-vol values using the existing alias semantics. Do not describe this as a
continuous, discretization-free Dupire derivative or a stochastic-rate joint fit.

The IVS has no clone contract. A snapshot must therefore retain the numeric base
IVS samples required by the fixed stencil and band selection, plus spot/rate/
dividend, rather than retain a caller-owned polymorphic object. Later replay
must not call the original IVS or depend on its continued lifetime. This also
supports a user-defined IVS without adding a new pure virtual method.

## Requirements

R01. Preserve the existing calibration and MC APIs and their default execution
cost. The new operation is opt-in and uses an independent calibration recording
after numeric MC gradient reduction. It never inserts calibration into workers.

R02. Copy quote axes/values, calibration settings and display name before sampling an IVS.
Require finite strictly increasing positive strikes/maturities and inclusion
grids, finite positive grid spacings, and matrix extents matching quote axes.
Reject nonfinite spread/base volatility and a nonpositive bumped implied vol.
Base volatility must be nonnegative. Grid fill extents must fit the existing
integer algorithm, and spacing below the coordinate's representable increment
must fail before sampling; otherwise a supplementary grid loop can stop advancing.

R03. Retain an immutable passive snapshot of the complete mathematical input:
base stencil samples, spot/rate/dividend, ordered quote axes/values, inclusion
and completed grids, fixed band indices, stencil version and output values.
Getters expose const data; no tape, Number_, recording or callback survives.

R04. A parameter seed includes the expected output grids and values/snapshot
identity as well as the spot-major/time-minor numeric adjoint matrix. Reject
wrong shapes, changed coordinates, reordered axes, different quote values,
different calibration settings or nonfinite seeds even when labels coincide.
Copied snapshots with the same complete content remain compatible; a name alone
is never an identity check.

R05. Replay the same calibration from the frozen base samples and active quotes.
Require its primal surface to agree with the retained numeric surface at
relative/absolute 1e-12 before reverse. Seed each surface node additively so
copied boundary nodes accumulate all contributions. Zero and negative seeds
are valid. Extract ordinary numeric quote adjoints, then close the recording.
Contracted arithmetic must not turn call-level ULP differences into a failed
second-difference replay. The [rounding decision](../api-notes/aad-dupire-replay-rounding.md)
requires a freshly evaluated scalar call primal, its existing active-expression
derivative and a bounded call-level disagreement before the unchanged strict
surface check. Never copy expected surface values into the recording.

R06. An optional direct quote contribution must carry the same quote axes and
values. Add it once, without scaling or re-normalizing either contribution.
Return calibration-only, direct and total gradients for audit. Report factors
remain a separate projection and cannot alter raw adjoints.

R07. Diagnose nonpositive/too-small discrete call curvature, invalid local
variance, nonfinite calls/derivatives and invalid replay separately. Do not clip
negative variance or introduce an unrequested curvature floor. Domain checks
are confined to the new checked calibration boundary, preserving the old API.
An invalid surface never yields a successful snapshot or quote-risk result.
The checked boundary rejects a centered strike-call difference at or below
`8 * epsilon * (abs(C_minus) + abs(C_plus) + 2 * abs(C_center))`; this detects
unresolved subtraction and does not replace or floor the curvature. It also
rejects zero local variance, whose square-root pullback is singular.

R08. The result identifies its raw decimal-volatility quote unit, fixed-input
boundary, discrete algorithm version, calibration snapshot and native method.
Repeating with different seeds preserves old results. Failure followed by a
valid request recovers through the existing recording lifecycle; independent
nested recordings continue to be rejected.

R09. Hybrid integration selects a local-vol component by its typed component
identity, verifies its surface against the snapshot and maps its model ordinals
through component parameter counts. Do not extract with d_label dictionary
lookups. Verify the structured result's full model snapshot/axis and require all
surface coordinates needed by the seed; a missing selected input cannot mean
zero sensitivity. Other model parameters remain fixed for this quote request.

R10. Validate dimensions, coordinate identity and capabilities before creating
the calibration recording. The first implementation is a scalar numeric VJP;
it does not materialize the full calibration Jacobian. Aggregate compatible
trade parameter seeds before one pullback; reject incompatible snapshots.

## Independent executable acceptance

1. Existing interpolation tests plus a hand-calculated interior weighted spread
   and flat-boundary case establish quote matrix orientation independently.
2. Flat rate-aware IVS: a parallel spread direction recovers the summed seed
   derivative of a flat local-vol surface within 3e-5 absolute per unit seed.
   Include boundary copied nodes, negative weights and an exactly zero seed.
3. Calibration-only VJP: compare every small quote bucket and a random direction
   with centered differences of g_surface^T C(q), with fixed steps
   2e-4, 1e-4 and 5e-5. Require at least two adjacent steps to satisfy
   absolute 3e-4 plus relative 1e-3. Declare these tolerances before coding;
   retain every step, including cancellation-limited steps. Cover flat and
   Merton bases, rectangular axes and nonzero base spreads.
4. Add an independently known linear direct-quote term. Check each separated
   contribution and their total, including quote-unit report scaling.
5. Freeze a user-defined IVS, mutate/destroy the original, and repeat the
   pullback. Results and original snapshot remain identical.
6. Reject same-size wrong/reordered axes, changed surface values, bad quote
   dimensions, NaN/infinite seeds, invalid domains and unsupported nesting.
   A-success/B-failure/C-success retains A and recovers C.
7. After the core boundary passes, fixed-path Hybrid tree/compiled valuation
   checks compare surface AAD against node bumps, then quote AAD against full
   bump/recalibrate/valuation. Use one worker and identical path numbers,
   smoothing and model time steps. Predeclare quote bump steps 2e-4/1e-4/5e-5
   and absolute 1e-3 plus relative 1e-3 on a small smooth payoff. Report sampling,
   grid and smoothing errors separately from derivative agreement.
   The independent surface-node check uses centered steps 2e-5/1e-5/5e-6,
   absolute 1e-4 plus relative 1e-4 and two adjacent passing steps. These node
   steps are declared before the complete-chain test implementation. Retain every
   row for both bases and evaluator modes. Use a smooth quadratic payoff so no
   smoothing or exercise-policy estimator obscures the calibration chain.
   Compatible portfolio seeds use the same predeclared full recalibration
   protocol over every quote bucket and a direction. Also require an exact
   power-of-two seed control. Separate double-precision reverses followed by
   addition are a roundoff diagnostic, not an exact real-arithmetic reference:
   the retained stencil's large intermediate derivatives can amplify different
   floating summation orders. Preserve that diagnostic and verify the actual
   portfolio derivative against independent prices without changing this
   finite-difference protocol.
8. Build OFF and combined diagnostics; run appropriate sanitizer, public,
   binding, generated-output and installed-consumer checks before publication.
   Measure calibration-only and complete-request cost after correctness. Keep
   the nine-target performance policy and all prior failed evidence unchanged.

## Subsequent required integration

The [language boundary](../api-notes/aad-dupire-bindings.md) controls the active
Python projection and public flat-BS convenience overload. Python supports both
the existing Merton IVS and user overrides, keeps the GIL while sampling them,
and releases it only for callback-free passive/native pullbacks. Every returned
mutable numeric value and surface is detached from snapshot storage. Required
handles and numeric configuration reject implicit bool/enum coercions. Retain
the current fixed quote-oracle protocol for language-level complete chains.

C++ public factory/pullback, Python keyword interfaces and Excel immutable
factory/getters follow the validated core boundary. A common passive calibration
pullback must also adapt the existing curve quote-risk axis/state/provenance
without replacing its inverse or changing its analytic/bumped semantics.
F02 multiple outputs, implicit calibration, stochastic-rate joint calibration,
delta-quoted moving strikes and quote Gamma have separate later requirements.

The [Excel boundary](../api-notes/aad-dupire-excel.md) requires a complete
worksheet chain, including model creation from frozen carry. New shared C++
factories provide validated Merton inputs and a detached one-factor Hybrid
model. Excel numeric-range conversion normalizes integer cells locally;
settings and native entries retain strict type/domain/context checks. All
getters and composite extraction avoid valuation/history work. Windows DLL
tests must scope the actual XLL worker state, distinct from the test runtime.
