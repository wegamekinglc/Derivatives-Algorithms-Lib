# Financial execution design critique

Verdict: Proceed with caveats.

The existing single-target builder cannot produce the requested simultaneous
Jacobian: substituting passive bases drops cross-layer derivatives. The design
addresses this with a separate active full-closure builder, verified using an
independent three-point layered reference rather than self-recovery comparison.

Binding requires RECORDING state. Register inputs, call StartRecording, bind
independent slots, then construct graph arithmetic. The preparation sequence was
corrected after checking the actual RecordingScope/native binding contract.

Significant constraints to preserve during implementation:

- Pricing metadata must retain actual row currency and native curve-coordinate
  units. Combining currencies or mapping to quotes changes the derivative.
- Shared base curves and aliases require one physical active curve and one
  independent selected slot. Unselected derived curves still transmit base risk.
- Fresh descriptor equality must precede compressed seeding; a matching name,
  shape, hash or current zero is insufficient. Pricing success is an additional
  gate, because available dependency proof does not establish valid fixings.
- Set and restore native mode outside the scope; failures and nesting must leave
  the next independent request usable. Do not hide partial-result failures.
- The numeric payload cap is limited to J plus directions. Check reused plans,
  byte overflow and int dimensions before those matrices are allocated; do not
  present this as a tape/RSS bound.
- Zero-color requests still validate finite pricing and current semantics.
  Native execution does not itself reject every nonfinite recovered gradient,
  so the financial result boundary needs an explicit complete finite scan.
- Dense and compressed timings must include the same pricing validation and
  full outputs. Dense should avoid unnecessary canonical identity construction.
  Retained plan reuse must still account for fresh complete identity capture.
- Keep the existing single-target pricing/sweep function bodies unchanged where
  possible; additive cold helpers make affected-path performance review clearer.

No blocking open user decision remains. The original default risk entry points
do not need to change to make explicit financial execution concrete and testable.
Automatic mode selection is not accepted by a sweep-count reduction alone.
