# Owning Black–Scholes segmented Monte Carlo boundary

Source: the native-only AAD roadmap, merged segmentation/curvature algorithms
and merged Python rate curvature #530 (`1aaa711e`). This is a new PR.

## Problem and goals

The core has sealed Black–Scholes script preparation, segmented mean gradients
and common-path finite-step Hessian products. Public C++ and Python lack an
owning financial preparation and passive result boundary for those algorithms.
Project the existing algorithms without changing their numerical method or
ordinary Monte Carlo defaults. LSMC policy curvature remains the next delivery.

## Requirements

1. A closed `BlackScholesMonteCarloPlan_` is constructed from a non-null script,
   valuation settings and smoothing width. It copies the contract before history
   resolution, prepares compiled native Black–Scholes execution and retains the
   immutable kernel, resolved valuation date, known observations and contract.
   Python exposes a factory, not a direct constructor or subclass callback.
2. The plan exposes parameter labels, original script constants, valuation,
   smoothing, original contract dates/events/settings and observation snapshots.
   All Python containers/settings are detached; shared fixing handles are immutable.
   Copy/deepcopy may share sealed native preparation. Source input destruction,
   global evaluation-date changes and Python getter mutation cannot change it.
3. Public mean-risk and curvature entry points take the plan, full numeric point,
   positive path count and optional segmented sampling settings. Curvature also
   requires the shared `BumpOverAADRequest_`. Columns are spot, vol, rate, div,
   then the plan's script constants, in native order and raw units.
4. Preserve all sampling fields: generator, bridge, absolute first path, optional
   Sobol scramble key and normal precision. Preserve segment length and optional
   checkpoint/recording capacity limits, including explicit zero limits.
5. Python settings are keyword-only and immutable. Reject bool/enums as integers
   or numbers, implicit numeric/string coercion, malformed sequences, nonfinite
   points and unrepresentable integers. Copy every argument before GIL release.
6. Delegate dimensions, domains, plus/minus admission, random-range overflow,
   checkpoint/recording caps, nesting, task draining and reduction to native
   algorithms. Unsupported EXERCISE contracts fail; no model/callback trampoline
   or experimental higher-order scalar is exposed. Native flags stay unchanged.
7. Mean results own value, gradient, point, labels, requested settings, execution
   and plan. Curvature results retain the same plan and expose native base,
   point, directions, actual steps, products, settings and execution. Detached
   getters cannot alter a result. Results survive collection of original inputs.
8. Metadata preserves normalized mean semantics, fixed 32-path batches, lane
   count, offsets, all sampling choices and tape/checkpoint/cleanup maxima.
   Curvature preserves `BumpOverSegmentedNativeAAD`, `1+2M` gradient requests,
   numeric payload and the effective minimum recording cap. These are per-path
   resources, not aggregate process memory or exact native second derivatives.
9. Empty directions preserve a full base gradient and a `0 x N` product.
   Numeric payload counts owning doubles according to the existing core formula;
   it excludes preparation, labels, random buffers, tasks and allocator overhead.
10. Plan creation resolves history once. Evaluation replays sealed observations
    and preserves historical dependence on explicitly supplied script constants.
    Independent calling threads own their evaluation state and release the GIL.

## Acceptance

- RED: focused C++ API test cannot compile against the merged baseline; focused
  Python factory test fails against the accepted installed module.
- Independent finite-path analytic Black–Scholes polynomial/linear payoff price,
  every raw gradient and actual-step analytic-gradient secants, including mixed
  and signed directions and a separate smooth Hessian convergence check.
- Nonzero offset and partial final batch; selected generator/bridge/precision
  and scramble cases; explicit zero-dimensional/time-zero/history replay.
- Empty/malformed dimensions, strict conversion, domain-crossing bump rejection,
  zero/short/exact budgets, invalid sampling and successful recovery.
- Immutable settings/containers, copy/deepcopy, garbage collection, explicit
  historical fixings, changed global date and GIL/independent-thread behavior.
- Build changed public and binding objects, installed consumption and strict
  OFF/combined probes of changed translation units. Reuse native archives only with verified source/header/
  configuration identities. Run new tests and selected actual callers only.
- Performance selection: new prepared base-only and mixed-direction complete
  boundaries; affected old code only if actual source/dependency analysis finds
  an execution change. Calibrate each loop to at least 25 ms; two rounds of ten
  alternating interleaved process pairs. Disclose unequal estimator contracts.
- Exact-head CI/Codacy and all reviews, actual platform runtime cases, two final
  audits and guarded merge. Begin LSMC work only after this PR merges.

Open questions: none blocking this financial projection.
