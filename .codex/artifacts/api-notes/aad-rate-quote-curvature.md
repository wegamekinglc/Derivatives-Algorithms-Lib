# Rate quote curvature API decisions

- `NewRateCalibration(spec)` has typed single-curve and same-currency joint
  overloads. It chooses native analytic inverse/Jacobian options and retains
  provenance; ordinary first-order calibration APIs remain separate.
- `RateCalibrationSnapshot_` uses private shared immutable data. Expose only
  passive numeric vectors and immutable owning provenance, not mutable market
  handles or the retained replay specification.
- `RecalibrateRateWithRisk(snapshot, quotes)` consumes the complete raw quote
  vector in provenance order and returns a new owning snapshot.
- `EvaluateRateQuoteCurvature(objective, snapshot, BumpOverAADRequest_)` follows
  the Dupire native scalar objective convention: concatenated free parameters
  and direct raw quotes. Reuse common pullback and scaled gradient differencing.
- Result getters expose value, raw point/gradient, directions/steps, HVPs, base
  calibration and execution counts/capacity diagnostics. Gamma uses a basis
  direction; cross-Gamma reads another coordinate of that product. No hidden
  dense Hessian, report scaling, fixed direct seeds or implicit unit conversion.
- Explicit trading adapters and cross-currency replay remain subsequent work;
  this increment supplies their recalibration primitive and smooth scalar kernel.
