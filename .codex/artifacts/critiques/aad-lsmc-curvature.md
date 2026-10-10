# LSMC curvature design critique

Verdict: Proceed with caveats.

## Blocking issues

None after requiring a single baseline policy for Frozen and point-specific
training for RetrainedBump.

## Significant concerns

- RetrainedBump differences a mixed first-order estimator. It does not establish
  a symmetric Hessian or O(h²) convergence when the fitted policy changes regimes.
  Preserve its inner-step rule and one-sided model boundary in provenance/docs.
- Changing constants must replay historical scalar/vector state before training
  and pricing. Reusing the original historical seed would miss real derivatives.
- Preparation owns a sealed observation plan but LSMC model data are ordinarily
  caller-owned. Construct each Black–Scholes model-data snapshot from copied
  numerical coordinates instead of retaining a mutable external model handle.
- Per-batch tape caps cannot be reported as request-wide aggregate caps. Restore
  scalar modes in worker scope and drain failed task groups before returning.
- Native fuzziness is piecewise, and training is hard. Validate finite-step
  estimators with independent passive paths and measured sampling dispersion;
  do not infer nonsmooth order from polynomial tests.

## Smaller alternatives and residual limits

Adding only a new label to repeated legacy calls would preserve the Frozen bug.
A generic model rebinding API would enlarge this increment unnecessarily. The
explicit Black–Scholes entry gives a useful, testable boundary while retaining
the existing native model's constraints. Binding parity and other models remain
separate work. No author question needs user clarification.
