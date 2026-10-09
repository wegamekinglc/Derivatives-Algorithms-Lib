# Native path segmentation review

Verdict: Comment Only.

## Findings

No unresolved local correctness or style findings. Publication acceptance is
pending the scoped cost study, complete external review and exact-head CI.
The financial adapter and whole-plan P05 acceptance remain open independently.

## Tests

Twelve focused cases pass. Independent analytic oracles check multiple states,
direct contributions, parameter-dependent initialization and terminal gradients,
fresh points, aliases and finite seeds whose weighted primal would overflow.
Validation covers empty shapes, admission arithmetic, exact/insufficient budgets,
same-value different-branch rejection, replay mismatch, failed-then-healthy
requests, mode restoration, preserved outer graphs and concurrent detached results.
A 16,384-step case verifies complete recomputation and a smaller local tape.

The first oracle has missing-header RED and passing GREEN evidence. A separate
error-context RED lacks the step label; GREEN repairs that label without losing
the shape constraint. Eight strict OFF/combined syntax checks and installed-only
consumption pass. Default and selected benchmark entry points pass their independent
price/gradient oracles. New core functions have complexity at most four; changed
benchmark functions have complexity at most seven. Documentation checks pass.

## Scope and residual risk

No existing production header/source is changed. All 179 accepted native archive
objects are reused byte for byte. The opt-in header does not enter ordinary
Monte Carlo, script evaluation, curve or PDE dependency paths. Existing timed
tape bodies remain unchanged; benchmark dispatch adds the new component cases.
The kernel must provide a complete fixed-size state and matching passive/native
formulas. Financial state liveness, smoothing, observations and RNG integration
still require their own implementation and acceptance.
