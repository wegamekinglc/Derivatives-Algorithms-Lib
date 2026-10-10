# Rate-trade curvature design critique

Verdict: Proceed with caveats.

Reviewed the active specification/API note, existing rate-curvature replay,
native financial pricing, parameter-Jacobian reconstruction and fixing snapshots.

## Blocking issues

None remain in the design. The following requirements resolve the identified
failure paths and must be demonstrated by tests before acceptance.

## Significant concerns

- The single-curve factory currently uses a non-owning alias while constructing
  provenance. Storing that alias would dangle. Transfer the calibrated unique
  pointer into shared ownership after provenance capture and before returning.
- Staged XCCY provenance registers only the free basis curve. Native financial
  reconstruction additionally requires its consumed fixed roots to be registered.
  Add internal fixed bindings after provenance, without altering risk axes.
- Native pricing's result label can differ from actual XCCY PV denomination.
  Require one actual currency and return it explicitly. A zero portfolio weight
  must not hide invalid terms, unavailable curves or inconsistent units.
- Calibration and trade fixing dependencies differ. Merge immutable observations
  once, reject conflicting saved values, and preserve explicit-snapshot semantics.
  Do not let later global mutations affect bumped prices.
- `RequestCashflows_` contains addresses into trade terms. Construct owned trades
  before preparing geometry and keep their storage stable. Use immutable prepared
  cashflows with request-local scratch; do not retain mutable per-call caches.
- A weighted sum of the existing full trade Jacobian wastes reverse directions
  and makes the existing execution counters untrue. Sum active PVs first and
  verify the one-reverse-per-calibration count with multiple trades.

## Minor notes and counter-proposals

Keep financial formulas and curve graph assembly in core, and keep the public
adapter as composition of typed capture plus the existing driver. A declared
unused objective-input tail permits the existing `[parameters, raw quotes]`
contract without parameter-vector slicing or a public generic market callback.
Keep fixed curve roots internal rather than adding public calibration coordinates.

## Author questions

None. Independent passive requoting, fixing-isolation, ownership, currency and
budget tests are acceptance obligations, not optional follow-up work.
