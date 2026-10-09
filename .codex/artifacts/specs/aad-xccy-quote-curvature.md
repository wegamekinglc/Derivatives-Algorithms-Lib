# Cross-currency recalibrated quote curvature

Status: active; extends the smooth exact-square rate primitive accepted in #523.
Source: F04 of [the implementation ledger](../plans/aad-implementation.md).

## Problem and scope

The rate snapshot can replay single and same-currency joint calibration, but
cannot yet represent either native cross-currency calibration family. Add both
families to the existing owning snapshot and curvature driver. Trading adapters,
approximate/rectangular solver derivatives, policy estimators and native mixed
mode remain separate required deliveries.

## Requirements

1. Add typed `NewRateCalibration` overloads for `CrossCurrencyCalibrationSpec_`
   and `JointXccyCalibrationSpec_`. Reuse the existing result, request, execution
   counters, resource controls and error context without changing their meaning.
2. Staged calibration exposes only raw basis quotes and free basis parameters;
   domestic/foreign curves and FX spot remain fixed dependencies. Joint calibration
   exposes domestic curves, foreign curves and basis in native residual/parameter
   range order. FX spot is fixed in this increment, outside the raw quote axis.
3. At every base/plus/minus point, perform a fresh native analytic EXACT solve,
   validate a nonempty square locally invertible Jacobian, build the appropriate
   owning provenance and apply its common pullback. Never reuse the base inverse.
   Cross-currency native weighted inversion can retain approximately 1e-10 identity
   error on a regular eight-parameter example. Apply one residual correction
   `E <- E + E * (I - J * E / tolerance)` to each fresh cross-currency inverse, then require
   the original dimension-scaled floating-point identity bound. This is a local
   square implicit inverse refinement, not a derivative of solver iterations.
   Retain the refined matrix in provenance; do not weaken the acceptance bound.
4. Preserve instrument configuration, notional/reset/fixing conventions, curve
   layering, collateral and projection routes. Normalize YC instruments once;
   retain native XCCY instrument order. Full quote/parameter axis fingerprints
   must remain identical across replay.
   Joint currency declarations receive stable per-currency ordinal prefixes in
   the sealed copy, so repeated user curve names cannot collide in native ranges
   or component bindings. Keys are `domestic:<ordinal>:<name>` and
   `foreign:<ordinal>:<name>`; preserve rejection of empty original names and
   leave the caller's original definition unchanged.
5. Seal fixed curve blocks with deep native graph copies, preserving shared
   aliases, route fallback and library day basis. Reject custom curve/block
   subclasses before invoking their virtual behavior. Existing legacy native
   single-curve blocks retain their routing semantics.
6. Resolve absent fixing snapshots once during factory creation from the exact
   required historical dependencies. Retain that immutable snapshot for every
   replay. Later global fixing changes, caller spec changes and caller-owned
   curve mutation must not alter a saved calibration's behavior.
7. Preserve caller tape mode on success/failure, active-recording rejection,
   numeric/tape budgets, owning results and point/stage-specific failures.
   Finite-step estimates do not enable native `higherOrder` capability.

## Inputs, outputs and compatibility

Objective inputs remain `[free parameters, complete raw quotes]`; the total
quote derivative includes freshly differentiated direct quote dependence once.
Staged and joint snapshots intentionally have different domains and axes.
No mutable pricing market is exposed. Existing single/joint callers retain the
same interface and numerical contract.

## Executable acceptance

- RED: the first focused staged factory test fails to compile against #523;
  GREEN: both new typed factories and replay produce owning, available provenance.
- Independently recalibrate with the original passive solvers and compare a
  financial cross-currency price's gradient secants at three finite steps.
  Include cross-block directions and direct quote dependence. Do not use the
  new snapshot/pullback in the passive reference.
  Use 1e-14 residual tolerance and Richardson inner price differences at
  1e-4/5e-5 for curvature (1e-5/5e-6 for gradients), with outer steps
  4e-4/2e-4/1e-4. Absolute acceptance stays 1e-6 for gradient and 2e-3 for
  curvature; tighter inner solve precision controls price-difference noise.
- Cover asymmetric quote counts/order, non-default projection tenor, layered
  curves, fixed/resettable/mark-to-market notionals where supported, and retained
  historical fixing behavior. Verify fixed curve mutation/route fallback,
  unsupported types, shape/mode admission and recovery.
- Rebuild only affected public sources/tests, run the previous rate cases,
  strict OFF/combined probes and an installed consumer. Reuse core/other facade
  evidence only with source/dependency/object/archive identity proof.
- Performance scope: staged and joint new entries versus matched explicit
  rebuild-gradient calls, plus one existing rate-curvature entry affected by
  the shared implementation. Keep calibrated sampling and regression thresholds;
  omit unrelated MC/PDE/Dupire/full parameter matrices.
- Complete current-head CI/Codacy/review bodies/comments and actual runtime
  coverage, then guarded merge with tested/merged tree equality.

## Open questions

None blocking this increment. General trade-objective adaptation remains the
next rate delivery after these two calibration domains are accepted.
