# Excel rate curvature design critique

Verdict: Proceed with caveats.

## Blocking issues

None after the following decisions are explicit in the specification and API.

## Significant concerns

- Existing results retain final specs and option overrides separately. The new
  native snapshot factory accepts specs only and uses native default options.
  Document and test the new solve; never imply reuse of fitted parameters or
  option overrides. Legacy result-only wrappers cannot reconstruct a spec.
- Native rate recording caps are supported, unlike the previous Dupire
  adapter. Delegate their enforcement, keep zero meaningful, and test recovery.
- Native snapshots seal dependencies, but portfolio settings must copy fixing
  observations rather than retain a mutable alias through an old wrapper.
- Zero-weight rows must still be checked. Dynamic trade-vector admission must
  report the bad row, with full native semantic admission delegated unchanged.
- Quote labels must follow native provenance, including joint declaration keys
  and staged-versus-joint XCCY axes. Return an existing calibration-risk plan;
  do not build a second quote formatter.
- Excel repository storage rejects a null output handle. Return copied fixing
  records with an explicit-presence row, preserving absent versus explicit empty
  history. Rate axis metadata deliberately omits quote values; obtain them from
  the separate Point query, and never dereference an unset optional value.

## Minor notes and counter-proposals

The native closed portfolio objective already handles currency, historical
fixings and all supported trade families. Adding a worksheet generic callback
or another solver would increase scope without closing a binding gap. Test
the adapter routes and independent smooth deposit reference; reuse accepted
native cross-family numerical evidence by provenance. Two rate-size cost cases
cover the new caller path; no old Dupire or unrelated native matrix is needed.

Author questions: none. Raw Windows export execution and exact-head review
remain required publication evidence, separate from portable local checks.
