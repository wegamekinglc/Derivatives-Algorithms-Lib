# Dupire quote-curvature cost acceptance

Status: selection frozen before measurement; results pending.

Base: merged #519 at `d2bd2ad22142929c30eb21ea25870ebbd2123b57`.
New implementation uses the accepted 182-member native archive, replacing only
the Dupire risk object and adding the quote-curvature object. Dependency and
member identity evidence must establish retained generic/MC applicability.

## Selected paths

- New flat-base parallel request: six quotes, eighteen local-vol nodes, one
  direction and three full calibration/gradient chains.
- New nonflat Merton-base request: the same grid, a parallel and a signed mixed
  direction, five complete chains.
- Existing first-order Dupire pullback: one small nonflat surface and complete
  node seeds. The changed Dupire risk translation unit contains this old caller,
  so its affected cost receives one paired control.

The two new cases compare the public driver with explicit manual composition of
the same passive recalibration, fresh objective AAD and fresh calibration AAD.
Both use the candidate primitive and precision, complete preflight, scalar mode
restoration, capacity scopes, owning outputs and result verification. This is
an informational new-method overhead comparison, not a historical speedup or
regression gate. The first-order control uses the immutable base archive.

Reuse the accepted generic and MC costs only with unchanged dependency/member
and executable proof. Exclude unrelated tape, solver, rate, PDE and portfolio
matrices: their algorithms and recording headers are unchanged. Required CI
continues to use its existing platform profiles.

## Sampling

For each selected pair: two rounds of ten process samples per side, alternating
base/head order within each round; one warmup and three timed repetitions per
process, best-of-ten reduction per round. The first-order control retains the
two-round +4% threshold. Save commands, configuration, CPU/compiler/environment,
source/dependency/object/executable hashes, all raw samples and verification.
Do not edit C++ or Git/PR state during sampling. Expand only for an identified
failure, unstable minima or new affected caller.
