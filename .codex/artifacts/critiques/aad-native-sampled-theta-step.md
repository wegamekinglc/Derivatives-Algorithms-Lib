# Native sampled-step design critique

Verdict: **Proceed with caveats**.

Numeric #504 is accepted and merged. The controlling SPEC/API explicitly include
the complete-coordinate numeric reverse domain described below. No blocking
issue or user clarification remains; native behavior still requires executable
acceptance before publication.

1. Layers and AAD channels are different axes. Each channel seeds all layers
   for one objective; coefficient/time risks aggregate layers. Forward errors
   are a vector of layers, reverse errors a layers-by-channel matrix. Reuse
   existing report identity/window collection, never flatten these axes.
2. Active field presence overrides that field's primal. Empty means passive,
   not a missing/invalid active shape. Validate complete replacement dimensions,
   finite values and slot lifetime before outputs. Signed rate/drift and variance
   units inherit numeric admission; theta endpoints retain theta risk.
3. Equal old/external values cannot infer provenance. Pass flags unchanged and
   retain all declared active external slot checks. Unused external contributions
   are zero, aliases sum once per formal contribution.
4. Never publish boundary output Numbers as aliases to old/external inputs.
   Separate output slots and one ordered event preserve seed consumption, serial
   composition and checkpoints.
5. Numerical admission must be explicit for partially active fields. The first
   wrapper may inherit the numeric cache's complete-coordinate Reverse domain,
   computing all physical risks before scattering only declared active slots.
   Then a range loss in an inactive contraction still rejects. Document and
   test that boundary rather than silently promising selective-range admission.
   A selective numeric reverse is a separate contract/performance change; do not
   add seven switches to a hot path merely to address a hypothetical use case.
6. Stage copies/capture under the caller budget before tape-owned event copy,
   without refactorization. Retained event/bindings and returned diagnostics/
   outputs have different owners; scratch measurements can overlap caller/tape
   limits. Cover exact peaks, one-byte-short failures and partial publication.
7. Use current generic checked event wrapper, scalar/vector modes and lifecycle
   failure semantics. No new pluggable backend, condition estimator, provider
   callback or mesh derivative is implied. Only actually changed helper callers
   need remeasurement; ordinary same-byte layouts preserve evidence.

The first scalar rate test has independent expected total risk 163/735 and a 1-by-1
actual transpose report. Later cases include field activities, direct expression
risk, aliases, serial steps, checkpoints and widths 1/4/8, source destruction,
nonfinite/range/lifetime failures and concurrent independent recordings. Complex
failure/resource cases get focused RED/GREEN evidence before production repairs.
