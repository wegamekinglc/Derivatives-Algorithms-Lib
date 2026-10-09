# Critique: native Dupire quote curvature

Verdict: Proceed with caveats.

## Blocking issues

None in the bounded design. The implementation must rebuild both calibration
and objective seeds at every quote point; a frozen pullback is unacceptable.

## Significant concerns

- Native Dupire uses numerical call stencils and a retained base-derived band.
  The estimator follows that fixed-band algorithm. Independent second-order
  differences need declared multi-step evidence and must disclose roundoff.
- Recording capacity admission must include both sequential native recordings,
  and mode restoration must be established before any allocating mode change.
- Copying `std::function` preserves reference captures. Input ownership cannot
  be presented as deep ownership of arbitrary callback state.
- Raw quote coordinates are distinct from report scaling. Mixed/report Hessian
  factors must not be inferred from a first-order projection.
- The native callback cannot safely contain parallel Monte Carlo. The full
  financial adapter remains a separate explicitly estimated delivery.

## Minor notes and counter-proposals

Reuse accepted robust secant arithmetic and expose passive recalibration
admission instead of evaluating every expensive perturbed objective twice.
Stream gradients and retain only the base calibration. Keep curve-provider
and binding completion visible rather than claiming F04 quote risk complete.

## Author questions

No user input needed. Require a nonzero calibration-curvature reference that
fails a frozen-Jacobian implementation before accepting the driver.
