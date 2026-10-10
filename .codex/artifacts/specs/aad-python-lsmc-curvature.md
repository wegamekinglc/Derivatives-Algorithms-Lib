# Owning native Black–Scholes LSMC curvature boundary

Source: the native-only AAD roadmap and accepted native LSMC algorithm #527.
Start from merged #531 (`b047d0b37be388b83b173720acfbfdef7cf533f2`).

## Problem and scope

Native LSMC curvature owns a prepared script and fitted base policy, but public
C++ and Python lack a closed financial preparation and passive result projection.
Expose that accepted algorithm without changing training, regression, fuzzy
exercise, gradients, sampling, budgets or ordinary Monte Carlo defaults.
Excel and the structured-operator interface audit remain separate deliveries.

## Functional requirements

1. Factory-only `BlackScholesLsmcPlan_` copies the non-null contract, simulation
   and valuation settings before resolving history. Construct the concrete
   native Black–Scholes model solely to bind the observation plan; evaluation
   takes the complete explicit numeric point. No external model creation,
   subclass trampoline, Python objective or active scalar enters execution.
2. Require native AAD and a live EXERCISE preparation. Reuse
   `DefaultRiskMonteCarloSettings()` for omitted simulation; an explicitly
   disabled AAD setting fails rather than being silently overridden. Preserve
   tree/compiled selection, smoothing, generator, bridge, precision, training,
   validation, RQMC replicate/seed, policy mode and inner relative bump settings.
3. Plans retain immutable contract/preparation and shared constant metadata.
   Expose raw parameter labels (spot, vol, rate, div, then script constants),
   original constants, resolved valuation, simulation, dates/events/contract
   settings, live event dates/time line and known observation snapshots.
   Copies have constant size. Python containers/settings are detached.
4. Public evaluation takes a plan, full finite point, positive pricing path
   count and the existing `BumpOverAADRequest_`. Snapshot every argument before
   worker submission and Python GIL release. Reuse checked native list/tuple
   numeric conversion, positive path count and the single shared bump type.
5. Results own the plan and native value, gradient, point, direction matrix,
   actual steps, HVP matrix, fitted base policy and execution. Expose every
   passive regression field: coefficients, normalization, basis powers/degree,
   degeneracy, rank/solver/fallback, condition count and validation error.
   Nested getters and copy/deepcopy cannot mutate the estimator; results survive
   input destruction and global date/fixing changes.
6. Frozen trains once and reuses that exact base policy for every outer point.
   RetrainedBump retrains at each outer point and differences the declared
   gradient estimator, including its inner policy-only price secant. Preserve
   both method names and separately expose outer steps and the inner setting.
   Neither mode is an exact analytic Hessian of optimal stopping.
7. Delegate domains, all outer/inner preflight, absolute stream ranges, common
   training/validation/pricing blocks, task draining, tape nesting/restoration,
   historical constant replay and reduction to the accepted native algorithm.
   Evaluation must never resolve history or read external fixings again.
8. Empty directions return the full base gradient and a 0-by-N product.
   Preserve explicit zero budgets. Numeric payload excludes retained policy,
   preparation and temporary work. Recording caps apply per fuzzy replay batch;
   tape/cleanup maxima do not represent aggregate memory or RSS. A recording
   failure can follow policy training; numeric/domain rejection precedes work.
9. Independent calling threads own evaluation state and release the GIL.
   Native higher-order flags remain false; no automatic step selection,
   Hessian symmetrization, strategy selection or new native algorithm is added.

## Executable acceptance

- RED against the merged baseline: missing C++ header and Python factory.
- C++ tests run with the ordinary uninitialized Google Test main and register
  DAL per case, matching CTest fresh-process isolation.
- Independent finite-path Frozen price replay over retained coefficients and
  normalization; finite differences of that same fixed policy validate every
  gradient and actual outer-step HVP. Mixed/signed directions and historical
  constant replay are included. Do not retrain a Frozen reference at bumps.
- RetrainedBump agrees with existing declared first-order valuations at the
  base and outer points, rebuilding original constant history consistently.
  Independently validate its base price; no generic C2 convergence is claimed.
- Tree/compiled, selected precision/bridge and training/validation/RQMC settings;
  full point/labels, policy field ownership, sampling counts and method metadata.
- Malformed/nonfinite/coerced values, path/count overflow, domain crossing,
  empty/exact/short/zero budgets, no live exercise, disabled AAD and recovery.
- Frozen observations/global-date isolation, detached getters, copies, GC,
  GIL heartbeat and two independent calling threads.
- Fresh affected public/binding builds, scoped strict OFF/combined probes and
  actual installed consumption. Reuse native and old boundary evidence only
  with source/dependency/configuration and executable identity.
- Select only complete Frozen and RetrainedBump Python boundary costs before
  timing. Two rounds of ten alternating interleaved process pairs, calibrated
  loops at least 25 ms. Disclose unequal minimal-native boundary contracts.
- Current-head CI/Codacy, all complete review bodies/threads, actual new-case
  platform logs, two final audits and guarded merge before the next stage.

Open questions: none blocking implementation.
