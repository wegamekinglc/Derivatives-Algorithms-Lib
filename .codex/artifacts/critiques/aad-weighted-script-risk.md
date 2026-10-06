# Weighted script risk contract critique

Verdict: Proceed with caveats.

The root increment has no blocking design issue. A scalar weighted expression
accumulates aliases through normal edges and records a path-local terminal node
even when all inputs predate the checkpoint. Separate suffix/prefix tests and
analytic derivatives are required; seeding several output references is not an
equivalent implementation when they alias.

Significant concerns controlling later integration:

- Resolve stable output IDs and the exact numeric payload before history,
  compilation or worker submission. The low-level root helper does not provide
  that request preflight and must not be advertised as a complete weighted API.
- A zero-weight component must still have a finite retained value. Validate every
  component before materializing the weighted expression, then reject overflow.
- Every active component must belong to the live caller recording. Default or
  stale numbers remain subject to native registration/lifetime contracts; this
  helper must not invent independent variables from passive values.
- The native driver already averages gradients. Weighted component and objective
  means must not introduce a second derivative normalization.
- Keep legacy scalar execution separate until integration proves unchanged
  forward work, output meaning and cost under the existing paired policy.

Minor note: the root takes two synchronous sequences and retains neither. The
later passive request/result types require their own copying and ownership tests.
No user answer is needed. Blocked Jacobians, exercise and portfolio timelines
remain outside the first weighted delivery.
