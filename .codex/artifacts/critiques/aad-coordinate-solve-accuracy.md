# Checked coordinate solve design review

Verdict: Proceed with caveats.

No blocking design finding remains. The following controls and numerical,
resource and changed-path acceptance still require implementation evidence.

## Findings converted to acceptance controls

1. **A dense full reverse would defeat packed risk storage.** Calling
   `CheckedLinearSolve_::Reverse` and projecting its matrix output also computes
   unused entries that can overflow. The declared implementation calls checked
   RHS-only reverse once and contracts directly. Exact full reverse allocation
   for the four-parameter fixture is 112 bytes on eight-byte-double platforms;
   allocating sixteen dense risks would violate that test. RHS-only is 80 bytes.
2. **Symmetric parameters affect two physical entries.** A zero off-diagonal
   value remains an active physical parameter. Analytic paired and zero-valued
   fixtures distinguish the correct sum from averaging, one-sided projection
   or treating zero as structurally passive. Paired-overflow rejection proves
   that individual finite entry contributions do not excuse an infinite sum.
3. **Accuracy reports can be checked against the wrong matrix.** Retain the
   already accepted checked cache of the expanded physical matrix. The
   independent rational rounding oracle accepts exactly at `2^-55`, rejects a
   `nextafter`-lower limit, and checks numeric recovery after failed reverse.
4. **Wrapper composition can duplicate factors and retained matrices.** Keep
   only layout metadata plus one checked dense cache. The expansion is temporary.
   The four-by-four fixture derives retained and peak limits from the existing
   checked cache plus precisely one live expansion; one-byte-short failures
   must refund all allocations.
5. **Sharing the contraction can alter ordinary path cost.** Extract one
   private inline helper preserving the accepted operation ordering and hoisted
   row ends. Compare archive members and fresh caller executables first. Reuse
   unchanged evidence; measure only callers whose compiled path actually changes.
   Added diagnostics/report work is reported separately from regression checks.
6. **A new suite could silently escape sanitizer filters.** New cases stay in
   `LinearSolvePullbackTest`, already selected in all sanitizer filters. The
   platform verifier must inspect exact-head actual pass lines for every new
   test rather than accepting green check status as proof of execution.
7. **Packed coordinates could be mistaken for a sparse solve.** Documentation
   states that LU, the physical transpose and condition diagnostics remain
   dense. The future PDE operator requires its own tridiagonal factorization,
   residual evaluation and numerical policy; this API supplies no PDE claim.

## Local resolution and remaining gates

Ownership/source-destruction, concurrent readers, malformed input/seed coverage,
legal custom pivot tolerance and exact construction/reverse ownership tests pass.
Strict compilation and installed usage pass; scoped existing-caller timing and
optional costs are retained separately. The
[local review](../reviews/aad-coordinate-solve-accuracy.md) records that evidence.
Final exact-head remote gates and actual fourteen-profile execution remain open.
The numeric PR must merge before the native collector integration increment.
