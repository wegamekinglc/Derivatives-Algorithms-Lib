# European PDE specification critique

Verdict: Proceed with caveats.

Blocking issues: none in the revised owning financial contract.

Significant concerns controlling implementation:

1. A scope constructor rewinds the default tape. Check for both an independent
   scope and a nonempty legacy graph before selecting mode or attaching a
   budget; a failed request must not destroy a caller graph.
2. Tape requirements depend on retained thread-local block capacity and ABI/
   diagnostic settings. Exact-limit tests must use comparable fresh threads
   and actual measured charges, including the cleanup reservation.
3. Terminal strike kinks are not ordinary differentiable coordinates. Reject
   exact grid-node strike and retain payoff-branch-aware centered differences.
4. Transpose reports are keyed by live events; copying entries in reverse
   visitation order would mislabel chronological diagnostics. Resolve each
   original event and copy passive errors before closing recording.
5. Moving the financial helper affects the old example/test callers even
   though no core solver translation unit changes. Rebuild those callers;
   prove unchanged core archive/configuration/dependencies before reuse.
6. Full numerical payload is 17+N+6S doubles under the chosen representation,
   not peak heap usage. Avoid a misleading memory-budget claim; verify exact
   arithmetic, exclusion wording and detached copies.

Minor notes: preserve two-integer aggregate example initialization; no source
comments referring to this plan; distinct price/risk, residual and continuum
acceptance tolerances. Python GIL proof must synchronize deterministically.

Counter-proposal adopted: keep one production financial helper with backwards
compatible example aliases, rather than another program in dal-public.

Author questions: none blocking. This verdict permits implementation under the
listed caveats; it is not numerical, performance or merge acceptance.
