# Cross-currency curvature design critique

Verdict: Proceed with caveats.

No blocking design issue after the following constraints were made explicit:

- A staged basis quote is not a joint currency-curve quote. Preserve domains,
  dependencies and quote axes; FX spot remains fixed rather than silently added.
- Never reconstruct pricing routes with a hard-coded tenor. Bind declarations
  through their actual collateral/tenor and owning solver results.
- Null fixing handles otherwise reread globals on each solve. Resolve once and
  seal before replay; a test must demonstrate isolation from later global changes.
- Fixed `CurveBlock_` maps can be empty for the legacy native single-curve form.
  Preserve its OIS/collateral/tenor fallback while deep copying its curve graph.
- Native YC solver order and XCCY declaration order differ. Normalize only the
  YC groups and preserve full axis fingerprints at every bumped point.
- Native joint XCCY accepts duplicate curve names in distinct collateral/tenor
  slots, while name-only provenance ranges collide. Prefix names in the sealed
  currency declarations with stable ordinals, consistently for native ranges
  and component bindings; preserve empty-name admission and caller definitions.
- Square shape alone is inadequate. Reuse fresh inverse/scaling validation;
  retain explicit rejection of rectangular/approximate semantics.
- A regular joint fixture exposes native weighted-inverse identity error around
  2.4e-10. Residual refinement may improve it, but must retain the original strict
  identity gate, fail closed when it does not converge, and pass independent
  financial price-curvature references. Do not relax tolerances to admit it.
- Independent price references must call the original passive calibrators.
  Comparing the new result with its own pullback cannot establish correctness.
- Price-difference truncation and solver residual noise are distinct errors.
  Preserve acceptance tolerances, use multi-step inner extrapolation and tighten
  solve precision before interpreting a discrepancy as an AAD error.

Significant concerns are floating-point conditioning and finite-step error;
multi-step financial references and exact-head sanitizer/Windows execution are
required. No author question blocks implementation. Avoid expanding this PR into
trade adapters or general solver differentiation.
