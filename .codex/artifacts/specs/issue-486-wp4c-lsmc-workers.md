# WP-4c: persistent LSMC fuzzy replay workers

Source: issue #486 WP-4 adoption order and the user's serial module/PR request.
Baseline is 226b4b3569fc6646bc2987fb244bd5a3f8e02eba (#561). Follow specialist
contracts locally; no agents are authorized.

## Problem and scope

Ordinary double/AAD now share persistent workers and bounded ordered partials.
LSMC fuzzy AAD and value-only policy replay still rebuild their model, RNG,
evaluator and recording once per 8192-path batch, and retain every batch's risk
vector. Extend the common worker helper to this complete replay module, including
its frozen/retrained curvature callers. Hard replay already reuses worker state.
Training, regression, policy sensitivity definitions and public signatures are
outside the behavior change.

## Requirements

1. Preserve BatchPlan_(paths, 1), its 8192-path boundaries and increasing batch
   reduction order. Keep every training/validation/pricing offset and RQMC key,
   normal precision, Brownian bridge option, default and risk normalization.
2. For each replay replicate use W=min(actual pool threads, replay batches)
   tasks. Each owns model, RNG, path, rows/sinks and evaluator as stack locals on
   its executing thread. AAD workers own one recording/prefix for that replay.
   Value-only policy passes use the same dispatcher and persistent ownership.
3. Seek each batch's absolute pricing offset on that worker's Sobol stream.
   Bind sinks after workspace construction, preserving return-by-value safety.
   Per AAD batch reverse the prefix, harvest raw parameter/constant risks and
   clear harvested input leaves. Final normalization remains once per replicate.
4. Reuse the common bounded ordered numeric partial storage: at most 256 slots
   and a 16 MiB numeric payload target, with one slot as the unavoidable minimum.
   Fold each batch directly into final values/risks without window regrouping.
   Preserve the existing ordinary dispatcher APIs and typed objective fallbacks.
5. Failures cancel future claims, wake slot waiters and drain accepted tasks
   before captured state is destroyed. Preserve nested recording rejection,
   owner-thread destruction, observers and independent concurrent requests.
6. Curvature keeps its numeric/tape budget admission, restored adjoint modes,
   base policy and point snapshots. Gather cleanup capacity only after recording
   closes on its owner thread; reduce worker statistics after all workers drain.
   Existing max-batch tape/cleanup fields keep their maximum-capacity meaning.
7. Profiling distinguishes one initialization per worker, one reverse suffix per
   path and one reverse prefix per replay batch. Include idle workers, bounded
   result storage and path/workspace accounting. Training phases retain their
   existing profiling and submission contracts.

Inputs/outputs remain MCLsmcAadSimulation, existing public valuation/curvature
surfaces and their current settings/results. No new tuning knob is introduced.

## Acceptance

- Actual baseline RED for replay task bounds and profiling ownership/counts,
  followed by unchanged GREEN tests. Keep training task counts independently
  measured through an otherwise identical hard-pricing control.
- Compare multi-batch AAD with separately fresh pricing segments, preserving
  the same fitted policy, historical active constants and absolute Sobol points.
  Prove this oracle against the baseline before relying on it.
- Exact cross-worker price/all-risk equality for tree/compiled, tails, historical
  state and RQMC; preserve frozen/retrained and curvature oracle/budget tests.
- Test bounded slot reuse with the legacy plan, cancellation/draining/recovery,
  capacity/mode failures and concurrent reuse. Rebuild and run full native/public/
  portable Excel tests, profiling tests and CLI contracts.
- Map changed callers to the smallest timed case set before measuring; retain
  two rounds x ten alternating pairs, min reduction and strict +4% in both rounds.
  Include multiple-batch BS/LV, dense risk RSS, value-only RetrainedBump, curvature
  and ordinary helper controls. Report every Greek and any last-bit differences.
- Review/documentation decisions, exact-head CI/Codacy, raw sanitizer coverage,
  two fresh paginated completion audits and guarded merge before later work.

## Open questions

No user clarification is required. Determine exact numerical parity and footprint
from executable evidence; old scheduler prototypes do not establish acceptance.
