# Cross-currency quote curvature review

Status: active; local implementation review and scoped performance acceptance
complete, remote exact-head acceptance pending. Verdict: Comment Only until
those gates finish.

## Findings

No remaining local correctness or interface finding in the changed production
files, installed consumer, test file and controlling specification/API/critique.
Review covers admission before virtual behavior, native quote/parameter order,
owning aliases, frozen historical dependencies, solver scaling, residual inverse
refinement, direct quote pullback and tape mode restoration.

Remote review found name-only joint bindings colliding for repeated declaration
names in distinct native slots. The regression test first fails with duplicate
provenance ranges while the original native calibrator succeeds. Stable ordinal
prefixes on the sealed declarations fix both native ranges and bindings without
changing core code or the caller's definitions. Empty original names still reject.
The regression also checks replay and independent financial price curvature.

The installed consumer exposed a default joint header reaching currency string
construction before validation. Explicit date/currency admission now rejects with
`Dal::Exception_`; a scoped malformed-header regression test covers both factories.
Native weighted-inverse roundoff on a regular joint system is handled by one
fresh residual correction followed by the unchanged strict identity gate.

## Open questions

None blocking the implementation. General trade adaptation, approximate or
rectangular solver derivatives, policy estimation and native mixed mode remain
separate deliveries; this PR makes no capability claim for them.

## Tests

- RED/GREEN factory overloads, default-header admission and inverse refinement.
- 24 affected rate cases pass: 12 accepted single/same-currency cases plus 12
  new XCCY cases. Independent references use original passive calibrators:
  90 curvature comparisons across three outer steps and 30 gradient coordinates.
  The name repair reruns only three joint and five shared XCCY cases, retaining
  sixteen unaffected single/same-currency/staged cases with unchanged caller paths.
- Staged fixed/resettable/mark-to-market notionals, layered/unlayered joint curves,
  asymmetric quote counts, non-default projection tenors, curve graph ownership,
  historical rate/FX fixings and legacy native single-curve route fallback pass.
- Active outer recording, caller-wide mode, capacity failure/recovery, unsupported
  native/custom shapes and malformed headers pass.
- Eight strict OFF/combined probes pass (four fresh after repair, four applicable
  unchanged probes retained); installed consumer passes 1/1. The public archive
  replaces only `ratecurvature.cpp.o`, retaining 25 identical members; core is unchanged.
- [Scoped performance acceptance](../performance/aad-xccy-quote-curvature.md)
  passes after repair: the same three comparisons retain 120 fresh samples and
  0.8805 seconds of measured work; changed candidate executable identities prevent
  reuse of earlier pair timings. Existing-entry round deltas +2.83%/+0.69% remain
  below the 4% gate. Current-head remote CI/Codacy/full review remain pending.

## Summary

The two typed factories reuse the accepted curvature primitive and preserve
distinct staged/joint axes. Residual risk is finite-step/conditioning behavior
beyond the independent financial fixtures and platform/sanitizer behavior until
remote execution finishes. Merge remains gated on those concrete checks.
