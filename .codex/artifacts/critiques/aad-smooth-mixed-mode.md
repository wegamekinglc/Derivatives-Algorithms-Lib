# Smooth mixed-mode design critique

Verdict: Proceed with caveats.

## Blocking issues

None in the specified opt-in scope.

## Significant concerns

- The existing tape stores passive local derivatives. Merely seeding it twice
  cannot produce a Hessian. Record the directional arithmetic itself as native
  active expressions and reverse its scalar root.
- A zero directional value is not an inactive graph. Analytic zero-point tests
  must catch pruning that loses nonzero second derivatives.
- Singular primitive derivatives can exist at finite values. Enforce the stated
  C2 domains, special-case passive powers zero/one, and diagnose reverse overflow.
- Repeated direction recordings require a deterministic callback. Snapshot
  numeric inputs and check scalar-value consistency, without claiming that these
  checks can prove arbitrary callback purity.
- A separate supported type does not make ordinary `Number_` recursively
  differentiable. Keep scoped capabilities and compile negative adapter probes.

## Minor notes and alternatives

Keep the new module isolated from first-order node layout and expression
templates. A fully templated new reverse tape would increase compatibility and
performance risk beyond this prototype. A forward scalar paired with the native
number reuses its established lifecycle and capacity controls.

## Author questions

None. Full-graph/opaque-operator differentiation requires a later specification.
