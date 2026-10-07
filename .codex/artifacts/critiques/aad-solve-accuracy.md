# Explicit solve accuracy critique

Verdict: Proceed with caveats; no blocking design issue.
Reviewed: approved plan, active numeric contract/API and accepted normalized LU,
diagnostic residual kernel, buffer accounting and nearby tests.

1. A physical transpose snapshot preserves exact input bits while reusing the
   existing residual kernel without per-reverse transpose allocation. Reconstructed
   LU entries would test a different matrix.
2. Require explicit finite policy limits, fixed before measurement; demonstrate
   equality and one-ULP-below boundaries with an independent rational oracle.
3. Full reverse may compute entry contributions before its residual check, but
   publishes nothing on failure. RHS-only must bypass unused entry overflow.
4. Preserve ordinary objects and callers; prove byte identity instead of retiming
   excluded workloads. New checking work is opt-in and must have disclosed costs.
5. Owning reports are per-call, so const reverse needs no mutable cache or lock.
   A single result with an empty RHS-only matrix keeps omissions distinguishable
   from computed zero matrix risk.
6. The floating report is not a certified forward-error bound. Keep large finite
   risks, conditioning and external parameter/method identities explicit.

Native report harvesting and implicit/PDE derivatives remain separate. Do not
claim this wrapper completes F03 or introduces a sparse/iterative solver.
