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
- 23 affected rate cases pass: 12 accepted single/same-currency cases plus 11
  new XCCY cases. Independent references use original passive calibrators:
  66 curvature comparisons across three outer steps and 22 gradient coordinates.
- Staged fixed/resettable/mark-to-market notionals, layered/unlayered joint curves,
  asymmetric quote counts, non-default projection tenors, curve graph ownership,
  historical rate/FX fixings and legacy native single-curve route fallback pass.
- Active outer recording, caller-wide mode, capacity failure/recovery, unsupported
  native/custom shapes and malformed headers pass.
- Eight strict OFF/combined probes pass (four fresh after repair, four applicable
  unchanged probes retained); installed consumer passes 1/1. The public archive
  replaces only `ratecurvature.cpp.o`, retaining 25 identical members; core is unchanged.
- [Scoped performance acceptance](../performance/aad-xccy-quote-curvature.md)
  passes: three comparisons, 120 samples, 0.8898 seconds of measured work.
  Existing-entry round deltas are +2.43%/+0.38%, below the unchanged 4% gate.
  Current-head remote CI/Codacy/full review remain pending.

## Summary

The two typed factories reuse the accepted curvature primitive and preserve
distinct staged/joint axes. Residual risk is finite-step/conditioning behavior
beyond the independent financial fixtures and platform/sanitizer behavior until
remote execution finishes. Merge remains gated on those concrete checks.
