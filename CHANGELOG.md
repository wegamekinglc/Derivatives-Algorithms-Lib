# Changelog

Notable, fundamentally-important changes to the DAL C++ quantitative finance library.
This file records **breaking changes, major new capabilities, new methodologies, and
significant methodology shifts only** — not every commit or minor fix. Routine refactors,
test work, formatting, and build/CI changes are deliberately omitted.

The library is documented as a single current version; the docs under `docs/` always
describe the latest state. This changelog is the only place historical context is kept.

## Entry format

Each entry is a short bullet under a dated heading, in the form:

- `<area>: <one-line description>` — link to the relevant doc or PR where useful.

Only add a heading when a qualifying change ships. Do not create empty future headings.

## Existing methodology and capabilities

These are documented today and represent the current documented surface; they are listed
here as the baseline rather than dated releases:

- **Automatic Adjoint Differentiation (AAD)** — reverse-mode AD for risk sensitivities, with
  Adept/XAD/CoDiPack backends. See `docs/methodology/aad.md`.
- **Yield Curve Construction** — discount-factor / forward-rate parameterised curves
  calibrated to market instruments. See `docs/methodology/yield_curve.md`.
- **Underdetermined Search** — constrained least-change solver for over-parameterised
  nonlinear calibration. See `docs/methodology/underdetermined_search.md`.
- **Cross-Currency Calibration** — XCCY basis-curve fitting across two currencies. See
  `docs/methodology/xccy_calibration.md`.
- **Analytic Jacobian for curve calibration (CurveJacobianMode flag)** — optional analytic
  Jacobian mode for yield-curve calibration. See
  `docs/experimental/aad-analytic-jacobian-curve-calibration.md`.

## 2026-06

- `curve`: Added a yield-curve Jacobian example demonstrating AAD-vs-bump agreement and the
  inverse-Jacobian IR-risk transform; documented the corrected units of
  `CurveCalibrationDiagnostics_::effJacobianInverse_` as
  `d(params)·tolerance_ / d(decimal-rate perturbation)` (the underdetermined solver scales
  residuals by `1/tolerance_` before forming the pseudoinverse, so consumers must divide by
  `tolerance_` when transforming a sensitivity vector: `r = gᵀ · effJacobianInverse_ / tolerance_`).
  See `docs/methodology/yield_curve_jacobian.md` and the example at
  `dal-cpp/examples/yield_curve_jacobian/`. Non-breaking (new example + diagnostics-only test).
- `curve`: Exposed the calibration forward Jacobian on the public diagnostics struct as
  `CurveCalibrationDiagnostics_::jacobian_` (and the `CrossCurrencyCalibrationDiagnostics_` mirror,
  empty on the xccy path for now) — the unscaled analytic `d(modelRate)/d(logDF_free)` at the solved
  point, populated iff `jacobianMode_ = ANALYTIC && solveMode_ = EXACT` and eligible. The
  `yield_curve_jacobian` example now reads the AAD Jacobian from `result.diagnostics_.jacobian_`
  instead of the `TestOnly::AnalyticJacobianAt` helper, and a new test
  (`dal-cpp/tests/curve/test_forward_jacobian_diagnostics.cpp`) gates population and AAD-vs-bump
  agreement. Non-breaking (additive public field).

<!-- Add new qualifying changes below as dated sections, e.g. -->
<!-- ## 2026-06 -->
<!-- - `curve`: Added log-linear interpolation to the interpolation module (non-breaking). -->
