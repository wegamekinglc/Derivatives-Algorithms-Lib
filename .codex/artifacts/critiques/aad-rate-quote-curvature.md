# Rate quote curvature design critique

Accepted with the following mandatory implementation checks:

- Rectangular effective inverses describe a weighted local mapping, which need
  not differentiate the solver's moving initial chart. Reject these cases.
- Native instrument subclasses can alter virtual rates. Exact-type checks must
  precede inspection, sorting, serialization and calibration.
- `Handle_` constness does not freeze a caller's mutable curve alias. Deep-copy
  bases with alias preservation; do not expose the retained graph through getters.
- Joint residual order differs from declaration order. Normalize and freeze it,
  and assert complete axis identity on every replay.
- Square shape alone is insufficient: validate the fresh analytic inverse using
  its documented solver scaling. Never infer correctness just from availability.
- Direct objective quote dependence is differentiated at every point, once.
- Tape capacity scopes must not nest. Calibration and objective phases admit
  separately under the same requested limit; restore adjoint mode on exceptions.
- Keep estimator claims finite-step and smooth-only. Independent price curvature
  must verify calibration curvature; comparing two uses of the same inverse is
  inadequate.
