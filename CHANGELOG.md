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
- **Interpolation** — linear, log-linear, cubic-spline, and mixed 1D interpolators plus
  bilinear 2D interpolation. See `docs/methodology/interpolation.md`.
- **Log-Discount Curve** — node log-discount-factor parameterisation with `LogDfScheme_`
  interpolation schemes (the parameterisation that supports the analytic Jacobian). See
  `docs/methodology/log_discount_curve.md`.
- **Yield-Curve Jacobian and Inverse-Jacobian Risk** — AAD forward Jacobian and the
  inverse-Jacobian IR-risk transform, including the `effJacobianInverse_` unit convention.
  See `docs/methodology/yield_curve_jacobian.md`.
- **Script Engine** — events-table to AST pipeline, visitor passes (domain analysis,
  constant-condition folding), and the fuzzy evaluator for pathwise AAD through
  discontinuous payoffs. See `docs/methodology/script_engine.md`.
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
- `curve`: Added a joint multi-curve AAD analytic Jacobian for `CalibrateJointMultiCurve`
  (`dal-cpp/dal/curve/jointcalibration.cpp`). The `JointResidualFunction_::Gradient` override
  produces a backend-neutral dense Jacobian via a single-result reverse sweep over the joint
  stacked parameter vector, using three new tape primitives: `Tape::DiscountPWLF_<T_,B_>`
  (PWL-forward curve with templated base handle, `dal-cpp/dal/curve/ycpwlf.hpp`),
  `Tape::JointCurveBlock_<T_>` (multi-curve routing context, `dal-cpp/dal/curve/jointycctx.hpp`),
  and `Tape::JointRate_<T_>` (projection-capable rate base, `dal-cpp/dal/curve/jointrate.hpp`).
  Eligible for specs whose declarations are all `PIECEWISE_LINEAR_FWD` with vanilla instruments
  (`Deposit_`, `FRA_`, `Future_`, `Swap_` -- `OISSwap_` included) and `liborBasis_ == ACT_365F`;
  base-layered forward curves propagate OIS adjoints through a templated `Number_`-typed base
  handle. The `JointMultiCurveCalibrationOptions_` struct carries a `jacobianMode_` field defaulting
  to `ANALYTIC` (matching single-curve), and `JointMultiCurveCalibrationResult_` now carries
  `jacobianAtSolution_` (populated under `ANALYTIC && EXACT && eligible`). The shared
  `XCurveJacobian_` (`dal-cpp/dal/curve/curvejacobian.hpp`) serves both the single-curve and joint
  paths. All four AAD backends (native, Adept, XAD, CoDiPack) verified with 750/750 tests passing,
  including 4 new oracle tests at `dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp`.
  See `docs/methodology/yield_curve.md` and `docs/methodology/aad.md`. Non-breaking (additive
  public surface; existing single-arg callers exercise the AAD path by default on eligible specs).

## 2026-07

- `script`: The compiled (flat-stream) evaluator is now the production default for
  `MCSimulation` in **both** specializations (`compiled=true` for `<double>` and
  `<AAD::Number_>`; `dal-cpp/dal/script/simulation.hpp`), and is at strict capability
  parity with the tree-walk evaluators: fuzzy smoothing (call-spread/butterfly
  kernels shared via `dal-cpp/dal/script/visitor/smoothing.hpp`, dt-blend `FuzzyIf`,
  per-condition `eps` overrides, `maxNestedIfs`), const variables (live `ConstVar`
  opcode preserving const-var greeks), past events, and `NodeCollect_` all produce
  the same numbers (tol 1e-8) through either path. `ScriptProduct_::Compile(fuzzy)`
  is now `const` and returns a `ScriptCompiled_` artifact; `MCSimulation` compiles
  internally, and `compiled` is `std::optional<bool>` (unset = library default).
  Exposed through `ValueByMonteCarlo` (`dal-public/src/value.hpp`) and the Python
  `MonteCarlo_Value` binding as a backward-compatible `compiled` keyword. **Breaking
  (behavioral defaults)**: (1) `AND`/`OR` are now eager in ALL evaluators — both
  operands always evaluate; scripts must not rely on short-circuit (conditions in
  this grammar are pure, so parseable scripts are unaffected); (2) evaluation
  defaults flipped from tree-walk to compiled (same numbers, ~20-25% faster);
  (3) `Compile()` signature/semantics changed from mutating member streams to a
  const artifact factory. See `docs/methodology/script_engine.md`.

- `random`: Sobol and PseudoRandom normal draws now default to the precise
  inverse-normal-CDF routine (`precise=true`) on `NewSobol`, `SobolRSG_`,
  `PseudoRandom_::New`, and `PseudoRSG_` (`dal-cpp/dal/math/random/{sobol,pseudorandom}.hpp`),
  restoring ~1e-15 Acklam+Newton accuracy in the default path (previously the
  faster ~1e-9 Acklam-only routine was the default). This shifts default
  Sobol/PseudoRandom normal variates; opt back into the fast path with
  `precise=false`. See `docs/methodology/random.md`. Non-breaking (default-argument
  reproducibility change only; the precise routine was already available).
- `random`: Sobol normal draws now skip the Newton polish on `InverseNCDF` by default
  (`SobolRSG_(..., polish=false)`, `dal-cpp/dal/math/random/sobol.hpp`), halving the per-deviate
  cost at the cost of ~1e-9 Acklam accuracy instead of ~1e-15 (Acklam+Newton) — below QMC sampling
  noise. This is a default-argument reproducibility change: default Sobol normal variates shifted
  from ~1e-15 to ~1e-9. Opt back in via `polish=true`. The new `polish_` member is serialized in the
  `SobolRSG` storable (`MG_SobolRSG_v1`). See `docs/methodology/random.md`. Non-breaking (additive
  public field; default changed for speed).

<!-- Add new qualifying changes below as dated sections, e.g. -->
<!-- ## 2026-06 -->
<!-- - `curve`: Added log-linear interpolation to the interpolation module (non-breaking). -->
