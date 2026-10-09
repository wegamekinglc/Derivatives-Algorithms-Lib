# Dupire quote-curvature API decisions

Status: proposed native C++ surface controlling implementation.

Use `ValidateDupireQuoteRecalibration(snapshot, spreads)` and
`RecalibrateDupireWithRisk(snapshot, spreads)` for passive domain admission and
complete numerical rebuilding from sealed base samples. Preserve the original
surface name, algorithm, fixed grids and quote coordinates. Avoid adding a new
raw IVS argument: it would permit inconsistent base sampling across bumps.

Add `EvaluateDupireQuoteCurvature(objective, calibration, bumps)` in a focused
model header. Reuse `AAD::BumpOverAADRequest_` for directions, steps and separate
numeric/recording budgets. The callback receives the complete calibrated local
volatility node vector followed by complete decimal-vol quote spreads. It can
therefore express direct quote dependencies and their mixed derivatives.

The result owns `Value()`, `Gradient()`, `Point()`, `Directions()`, `Steps()`,
`HessianProducts()`, `Calibration()` and `Execution()`. Product rows follow
submitted directions; columns follow complete raw quote coordinates. Selected
report projections and binding-specific labels are deferred to their explicit
integration layer. Report-scale Hessians require both coordinate factors;
directions are never silently rescaled by this native entry.

Reject a null objective, incompatible shapes, nonfinite values, ineffective
bumps, invalid calibrated domains, budget excess and nested recording before
objective execution where admission can determine the error. Evaluation errors
include base/direction/sign and stage. Public owning result construction must
preserve its snapshot invariant; prefer construction by the evaluator.

Existing native scalar callbacks are reused only for smooth sequential native
objectives. Full segmented Monte Carlo composition requires an outer financial
adapter, as in #519, and must not be nested inside the callback.

Rejected alternatives: retaining one calibration pullback across all bumps;
resampling the raw base IVS for each quote point; silently selecting steps;
materializing every bumped surface before evaluation; and advertising native
mixed-mode support through this finite-difference entry.
