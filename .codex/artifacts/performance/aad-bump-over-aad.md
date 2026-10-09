# Native bump-over-AAD scoped cost control

## Selection before measurement

The new driver is additive: ordinary tape, first-order pricing, calibration,
random/path generation and existing recorded operators are unchanged. Extend
the existing `tape_perf` target with one explicit curvature command. Select only
two whole requests: four inputs/one direction and 32 inputs/three directions.
Each uses a smooth coupled polynomial with an independent analytic Hessian,
fresh native recordings, explicit steps, owning result metadata and budgets.

Use identical complete-request benchmark harnesses on isolated frozen baseline
`faccea0b0d3a4112329436ede84cd7ec90ebf83d` and implementation builds. The baseline
reference performs the same 1+2M independent native gradients and central secants;
match result ownership, request copying and tape-capacity scope. Report this as
new-entry overhead, since the baseline has no native curvature API. One selected
existing segmented short request covers the shared benchmark dispatch boundary.
No ordinary native library hot body changes, and no unrelated target is selected.

For each selected comparison, retain two rounds of ten interleaved process
samples per side, alternate first side, reduce by minimum and apply the existing
+4% sustained threshold to the comparable old caller. New capability overhead is
reported separately. Keep full values/gradients/products and work counts in each
raw sample. Explicitly enable benchmarks in both Release CMake configurations.
Record source/dependency/archive/executable hashes, compiler, host/affinity and
thread settings. Timing is read-only after correctness passes and identities freeze.

Peak tape payload and cleanup reserve are separate; numeric payload is exactly
the documented owning doubles. Neither bounds allocator overhead, stacks or RSS.
Exclude RNG/PDE/curve/LSM, a dense Hessian matrix sweep, alternative worker counts
and unchanged heap matrices. Expand only for a concrete failed selected case.

## Acceptance state

Measurement is pending. No performance pass or regression claim is made yet.
