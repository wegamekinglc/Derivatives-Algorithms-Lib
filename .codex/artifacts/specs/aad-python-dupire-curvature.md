# Python Dupire quote curvature

Status: active implementation. Source: the remaining binding requirement in
[the implementation ledger](../plans/aad-implementation.md), after merged #528.

## Problem and scope

The accepted `PlanDupireScriptCurvature` / `ValueByMonteCarloWithDupireCurvature`
C++ financial adapter has no Python projection. Python users can obtain its
first-order chain but cannot request the corresponding recalibrated common-path
Gamma, cross-Gamma or Hessian-vector estimates. This increment exposes that
existing algorithm with owned requests/plans/results; it adds no numerical
method and does not expose active AAD numbers or Python recording callbacks.

Rate-trade curvature, structured operators, segmented/LSMC adapters, Excel and
the smooth forward-over-reverse prototype remain separate deliveries. The
prototype's existence does not change this adapter's finite-step method label.

## Requirements

1. Register the passive native `BumpOverAADRequest_` with keyword-only required
   `directions` (`DoubleMatrix_`) and `steps` (list/tuple of real int/float), then
   optional numeric-payload and recording-capacity byte budgets. Exclude bool,
   enums, strings, generators and implicit container-to-matrix coercions.
   Budgets accept nonnegative size_t index integers or None; zero is a real cap.
   Directions must be finite; steps finite and positive, one per direction row.
   The quote-dependent column count is checked by the existing native planner.
2. `DupireScriptCurvatureRequest_(risk=..., bumps=...)` requires the exact native
   first-order and bump request types. Copy inputs, expose detached read-only
   properties and support copy/deepcopy consistently with existing requests.
3. `DupireScriptCurvaturePlan_New(product, modelData, calibration, component,
   request)` checks all Python inputs under the GIL, owns their native copies,
   then releases the GIL for native planning. The plan exposes its base plan,
   full point, directions, steps and admitted payload without mutable aliases.
4. `DupireScriptCurvatureResult_New(plan)` accepts the exact plan type, releases
   the GIL for execution and returns an owned result. Expose base financial
   result, point, raw gradient, complete input axis, directions, steps,
   Hessian products and execution method/count/path/payload metadata.
5. Preserve native full raw strike-major decimal-vol coordinates. First-order
   selected inputs and report factors affect only the nested base projection.
   Preserve signed direction rows and empty-direction base-only requests.
   An all-zero direction row rejects under the native finite-step contract.
6. Preserve calibration/history snapshots, RNG/path/settings identity, repeated
   evaluation, failure recovery and rejection of external first-order direct
   seeds, EXERCISE and worker recording-cap requests. Do not silently drop caps.
7. Python errors identify the function/type and offending field. Native dimension,
   perturbation, calibration and payload errors retain their existing context.
8. Existing entry points, numerical libraries and capabilities stay unchanged.

## Acceptance and performance boundary

- RED against the installed baseline: the public constructor is absent.
- Analytic direct-quote quadratic: raw Gamma `2*exp(-0.05)`, signed rows and a
  direction with zero product, both tree and compiled modes; verify work `1+2M`,
  paths and exact payload. Reject an all-zero direction row.
- Independently rebuild plus/minus first-order quote gradients using existing
  Python calibration/planning entries on common paths and compare every HVP
  coordinate. This verifies column order and recalibration without a new core.
- Check selected/reported base versus full raw axes, copied inputs/getters,
  copy/deepcopy, garbage collection, repeated evaluation and thread/GIL behavior.
- Test wrong types, budget overflow/zero/exact/one-byte-short, nonfinite values,
  malformed dimensions and unsupported settings with valid-request recovery.
- Run new tests and the directly affected existing Dupire request/risk tests.
  Use standalone installed-package consumption and strict changed-unit checks.
- Benchmark only the new complete Python plan/execute boundary at base-only and
  multi-direction shapes, using two rounds of ten interleaved pairs and calibrated
  sampling. Compare with equivalent existing first-order composition; labels
  state that planning/admission differs. No general acceleration claim.
- Reuse unchanged native acceptance only with library/header provenance. Analyze
  binding callers; do not repeat unrelated native/PDE/rate/portfolio matrices.
- All current-head required CI, Codacy and actionable reviews must be clear,
  with repeated final audits before guarded merge. Inspect actual relevant Python
  platform logs; a collected test name alone is not an executed pass.

Open questions: none required for this bounded projection.
