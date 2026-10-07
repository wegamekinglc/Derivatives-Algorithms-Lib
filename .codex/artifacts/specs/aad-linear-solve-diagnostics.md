# F03 native recorded solve diagnostics

Status: active implementation after merged numeric diagnostics PR #493 at
`c3f939302b2066b8b1f120e0bf513ec3b6cb8fc1`.
Source: the detailed AAD plan, native-only scope, sequential PR delivery and
project-wide requirement to validate performance only where a change has impact.

## Problem and delivery boundary

The numeric API owns a solve and passive diagnostics, but native recordings
cannot return those diagnostics together with differentiable outputs. Add that
explicit operation while retaining the ordinary solve's allocation, node layout
and mathematical behavior. Structured coordinates, implicit calibration/PDE,
sparsity, long-path checkpointing, second order and bindings remain separate.

## Inputs and outputs

`Dal::AAD::LinearSolveWithDiagnostics(recording, A, B, tolerance)` has the same
three active/passive combinations and default pivot tolerance as `LinearSolve`.
The new header is `dal/math/aad/linearsolvediagnostics.hpp`. It returns
`DiagnosedLinearSolveResult_` with `solution_` (`Matrix_<Number_>`) and
`diagnostics_` (`LinearSolveDiagnostics_`). The report owns passive values; it
does not create diagnostic Number nodes or differentiate a condition metric.
A fully passive solve uses the numeric API.

## Numbered requirements

1. Preserve ordinary recording preconditions, owner-thread validation, input
   binding validation, shape/finite checks, pivot policy and graph invalidation.
   Diagnostic inverse-range failure additionally invalidates this operation's
   recording, even when an ordinary solve with that RHS would succeed.
2. Construct one owning `DiagnosedLinearSolve_` from captured values. Reuse its
   LU for forward, diagnostics and all reverse sweeps; do not refactor, copy LU
   or retain the temporary inverse.
3. Share payload/event reverse logic through a cache-type template. The ordinary
   specialization retains its existing field order, numeric cache and work.
   Number, native node, tape layout and ordinary scalar hot paths are unchanged.
4. Compute the same infinity-norm reciprocal condition and per-RHS componentwise
   errors as the accepted numeric diagnostic. No extra rejection threshold,
   regularization, certified forward bound or higher-order capability is added.
5. Snapshot inputs and bindings once. Outputs and gradients compose with native
   producers/consumers, all three activity combinations, aliases and multiple
   RHS, including positive, negative and zero channel seeds.
6. Cache the report inside the event and copy it into caller-owned storage
   outside the event allocation scope. Never transfer an event-accounted
   allocation to the returned report. The detached report remains readable after
   input-container changes, checkpoint restoration and recording close. Native
   outputs retain the existing recording lifetime contract.
7. Charge cached diagnostics and construction scratch to the tape account;
   charge detached reports and outputs to an attached caller buffer budget.
   Admit the report before publishing output nodes or committing an event.
   Copy/allocation/admission failures refund charges and invalidate the graph;
   the next independent recording must recover.
8. Repeated scalar/vector sweeps, clear-adjoints, prefix/suffix reversal,
   restoration and close use the existing event ordering and release rules.
   Release suffix reports/caches without affecting detached reports or prefix
   caches. Report values never change during reverse.
9. Keep ordinary public includes unchanged by putting this optional API in its
   own header. Native and numeric diagnostic APIs use the same vocabulary.
10. Accept only after focused tests, resource and local review, affected
    performance evidence and exact-head CI/Codacy/review gates. Subsequent
    delivery starts in a new PR after this PR merges.

## Executable acceptance

- Initial RED: A=2q, B=p*p, V=3X*X+2p at q=2,p=3. Assert X=2.25,
  dV/dp=22.25, dV/dq=-15.1875, reciprocal condition=1 and backward error=0;
  repeat a negative seed, then read the owning report after close.
- Three overloads, nonsymmetric multiple-RHS analytic/numeric diagnostics,
  independent finite-difference/directional gradient checks, aliases and widths
  1/4/8. Verify diagnostics and outputs use the same values and tolerance.
- Ordinary and diagnosed events compose across checkpoints; suffix restoration
  releases cached report storage, detached reports survive, and prefix gradients
  accumulate correctly through repeated suffixes.
- Exact tape/buffer peaks and one-byte-short failures; distinguish cached and
  detached report charges. Report budget rejection publishes no new output
  nodes/events. Test diagnostic inverse-range rejection and independent recovery.
- Run the focused solve/event/resource tests locally. Inspect actual execution
  of new tests under OFF/ON lifetime/profiling, Windows and sanitizer CI. Reuse
  existing sanitizer suite selections rather than growing the job matrix.
- Compare affected ordinary solve functions against the merged baseline with
  identical flags. Byte-identical existing runtime can reuse accepted evidence;
  changed runtime needs scoped paired measurements under the existing 4% policy.
  Diagnostic cost cases cover n=2/32, RHS=1/4 and scalar/vector reverse boundaries.
  Do not repeat unrelated portfolio/Monte Carlo/PDE benchmark collections.

## Open questions

None requires a user decision. The result owns passive diagnostics while its
native output handles retain the ordinary scope lifetime. All later F03/F04
requirements remain active and cannot be marked complete by this increment.
