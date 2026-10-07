# F03 dense solve scope critique

Verdict: Proceed with caveats.

## Blocking issues

None for the independent numeric boundary. Recording integration is not approved
by this critique and needs its own lifecycle/event-order design.

## Significant concerns

- A small normalized pivot is not a condition number. The specification names
  the diagnostic and rejection rule accordingly; tests must not claim that all
  ill-conditioned matrices are rejected or that accepted matrices are accurate.
- Transpose solving with row pivoting is the main correctness risk. Symmetric
  examples cannot reveal the error: require nonsymmetric cases with multiple
  swaps and an independently multiplied transpose residual.
- Dense coordinates differ from symmetric/shared storage coordinates. The
  result must remain an ambient dense gradient until an explicit later mapping
  is implemented and independently tested.
- Finite input does not imply finite intermediate products. Both solves and
  outer-product accumulation must reject overflow without invalidating immutable
  factors or publishing an incomplete gradient.

## Minor notes

Keep scalar-reference timings outside correctness tests. A new solver is justified
only within this opt-in boundary: do not replace existing decompositions or change
their regularization, singularity or performance policies.

## Counter-proposal considered

Accepting an arbitrary existing decomposition would save factorization code, but
cannot establish a reproducible dense pivot policy from the current interface.
The owning normalized LU keeps the first prototype's mathematical and failure
contracts explicit. Reuse of existing structured factors remains a later decision.

## Author questions

None required before the focused RED. The controlling
[specification](../specs/aad-linear-solve-pullback.md) already limits the initial
surface and records the remaining F03 requirements.
