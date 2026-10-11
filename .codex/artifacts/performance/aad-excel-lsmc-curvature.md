# Scoped LSMC worksheet cost acceptance

Status: selection before implementation/measurement, after merged #538 at
061e4673440898ef59c20a091f5c70309985c1f4. No timing pass is claimed yet.

Changed paths are the new LSMC adapter and a focused shared MC input/projection
header. The existing native/public/Python algorithms do not change.

- New Frozen case: one future positive polynomial EXERCISE, small explicit
  training count, 33 pricing paths, two signed mixed directions. Compare the
  accepted native owning evaluator with the Excel owning result and numerical
  projections. Contracts differ in ownership, so the cost is informational.
- New RetrainedBump case: the same small contract/shape with explicit inner
  policy bump, preserving the complete declared estimator. This is the other
  native algorithm mode, not a full Cartesian matrix. Check independent value,
  every gradient coordinate and actual-step products before/after batches.
- Affected existing MC: the accepted one-path mean and 33-path/two-observation
  curvature cases. Rebuild after helper extraction. Reuse accepted timings only
  if source/dependency/configuration provenance and executable identity establish
  applicability. Otherwise compare these two owning Excel contracts against
  merged #538 with the existing 4% regression gate.

Preserve two rounds of ten alternating fresh-process pairs, calibrated batches
of at least 25 ms, CPU/thread controls, raw observations and minima. Reduce case
count rather than weakening per-case evidence. Save immutable baseline/head,
build/dependency/executable hashes and exclusions in the final report.

Unchanged tape, RNG, rate, Dupire, PDE, generic reverse and Python matrices are
excluded locally. They do not include the focused helper or the new adapter.
Required exact-head CI still applies. Expand only for an actual failure or
identified caller gap, recording the reason first.
