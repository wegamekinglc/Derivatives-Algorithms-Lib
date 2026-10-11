# Excel native LSMC policy curvature

Active increment after merged [#538](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/538).
The accepted public `dal-public/src/lsmccurvature.hpp` owns preparation and
delegates numerical behavior to native LSMC. This increment provides worksheet
ownership, complete passive policy projections and strict raw admission.

## Requirements

1. Expose an immutable BlackScholesLsmcPlan and owning curvature result. Reuse
   Product, valuation, MonteCarloSettings and BumpOverAADRequest handles. Omitted
   simulation uses DefaultRiskMonteCarloSettings(), which enables native AAD;
   an explicitly supplied ordinary settings handle with AAD disabled rejects.
   Plans retain the copied contract, valuation, simulation, labels, constants,
   observations, event dates and time line. Native EXERCISE dates must be
   strictly after the evaluation date; same-day exercise is unsupported.
2. Read every physical cell of the complete finite point row/column. Reject
   leading, interior and trailing gaps, text, bool, date, errors and nonfinite
   numbers before raw conversion; normalize valid integer cells. Positive paths
   are exact size_t integers through the worksheet 2^53-1 limit. Preserve native
   training + validation + replicates * pricing <= UINT32_MAX admission and the
   existing settings constructor's int-based limits; add no INT_MAX price limit.
3. Delegate once to ValueByBlackScholesLsmcWithCurvature. Preserve Frozen and
   RetrainedBump methods, retained sampling and seeds, common absolute blocks,
   inner policy bump and distinct outer actual steps. Return value, all gradient
   coordinates, point, directions, steps, products and logical shape. General
   native higher-order capability remains false.
4. Preserve all ten native execution fields. Gradient evaluations count 1+2M
   complete gradient requests, not reverse sweeps. Recording caps apply per
   replay batch; tape/cleanup maxima are capacities, not concurrent memory/RSS.
   Numeric budgets exclude preparation, policy, metadata and worksheet copies.
   For N=5, zero directions retain 0 x 5 and 88 bytes; two directions use 264.
5. Expose indexed detached base-policy handles in native exercise-day order,
   with coefficients, normalization means/sigmas, three-slot basis powers and
   all ten scalar/optional native fields. Configuration also reports array
   lengths, allowing physical empty spills to retain logical 0 x 3 powers.
   Policy ordinals are zero-based and are not invented event-date IDs.
6. Add a full configuration getter for the existing MonteCarloSettings handle:
   all fourteen settings, preserving optional unset versus explicit false/zero.
   No new simulation settings type or constructor. All passive projections do
   no training, pricing, history lookup or task submission, and return copies.
7. Factories replace output handles only after complete success. Null/wrong
   handles, NUL text, input/domain/budget failures and nested caller recordings
   preserve prior results, caller mode and seeds; subsequent valid calls recover.
   Every new plan/result/policy risk handle rejects serialization. Existing
   legacy settings/archive behavior remains under its original contract.
8. Share MC's complete point/path readers and observation projection through a
   focused helper header included only by MC/LSMC adapters. Preserve MC behavior;
   do not duplicate parsing or broaden a widely included common header.

## Executable acceptance

- Establish missing-interface RED, then independent future polynomial EXERCISE
  value, complete gradient and signed mixed actual-step products in both modes.
  At zero vol/rate/div, spot=10 and SCALE=2 gives value 200, spot gradient 40 and
  SCALE gradient 100; direction [1,0,0,0,.5] gives spot/SCALE products 14/20.
  Verify vol/rate/div coordinates using actual pricing normals and elapsed time,
  and actual-step gradient secants rather than replacing them by exact Hessians.
- Focused cases cover compiled/interpreted, future discounting, captured history,
  full settings/policy copies, empty directions, exact numeric and separate
  recording caps, invalid raw/typed inputs and caller-state preservation.
  Zero-volatility finite-path vol adjoints require actual pricing normals;
  do not substitute an infinite-population zero mean.
- Initialize both executable and Windows XLL registries before typed planning.
  Confirm actual raw calls of every new export, exact registration/help, official
  generation/drift, strict OFF/combined probes and public binding boundaries.
- Retain affected MC typed cases. Follow the
  [cost scope](../performance/aad-excel-lsmc-curvature.md); reuse accepted native
  mathematical evidence only with checked source/dependency provenance.
- Inspect applicable current-head CI, actual runtime logs, full review bodies,
  Codacy and all threads; audit twice, merge by SHA and compare accepted/merged
  trees. No new native policy algorithm or callback API belongs to this scope.

No user decision is outstanding.
