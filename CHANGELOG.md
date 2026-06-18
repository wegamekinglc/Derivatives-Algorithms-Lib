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

<!-- Add new qualifying changes below as dated sections, e.g. -->
<!-- ## 2026-06 -->
<!-- - `curve`: Added log-linear interpolation to the interpolation module (non-breaking). -->
