# Native bump-over-AAD design critique

Verdict: Proceed with caveats.

## Blocking issues

None after making finite-difference identity, explicit steps, matrix orientation,
complete preflight and independent-recording lifetime part of the contract.

## Significant concerns

- Raw second-order pathwise differentiation is not generally an unbiased
  financial Gamma estimator. Limit this increment to declared deterministic
  smooth callbacks; later MC delivery must specify common paths and smoothing.
- Bumping a frozen quote pullback omits calibration curvature. Quote delivery
  must recalibrate every perturbed quote point and rebuild its first-order map.
- Small steps can disappear for only one component of a mixed-unit direction.
  Validate both sides of every nonzero component before any callback.
- Finite gradients can overflow during subtraction or `2h` formation even when
  the quotient is representable. Use exponent-scaled subtraction/division and
  independent extreme-value tests, including subnormal steps.
- A finite-step Hessian need not be symmetric. Preserve the directional result
  without averaging columns; h/2 convergence remains a caller/validation choice.
- Copying a callback cannot freeze objects referenced by its captures. Require
  fixed external state and distinguish that contract from numeric input snapshots.
- Per-recording scope cleanup must precede capacity/mode restoration on all
  exits. Test failures at base, plus and minus, nested calls and later recovery.
- Existing recorded solve/root operators require an explicit recording scope.
  A numbers-only callback cannot compose them. Supply the owned scope pointer,
  forbid callback lifecycle changes, and revalidate state before reading its root.

## Minor notes

Report numeric payload and tape capacity separately. Keep Gamma Taylor factors
and public report scales outside this native-coordinate primitive. Empty
directions still need an N-column matrix and return a base gradient.

## Counter-proposal

Do not add dense-Hessian or automatic-step convenience APIs in this PR. Basis
directions already request columns; selected directions preserve bounded work.

## Author questions

None. Financial estimator promotion and exact native mixed mode remain separate
requirements, not capabilities inferred from successful smooth-kernel tests.
