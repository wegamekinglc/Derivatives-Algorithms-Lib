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
  interpolation schemes and scalar-generic passive/AAD evaluation. See
  `docs/methodology/log_discount_curve.md`.
- **Yield-Curve Jacobian and Inverse-Jacobian Risk** — AAD forward Jacobian and the
  inverse-Jacobian IR-risk transform, including the `effJacobianInverse_` unit convention.
  See `docs/methodology/yield_curve_jacobian.md`.
- **Script Engine** — events-table to AST pipeline, visitor passes (domain analysis,
  constant-condition folding), and the fuzzy evaluator for pathwise AAD through
  discontinuous payoffs. See `docs/methodology/script_engine.md`.
- **Analytic Jacobian for curve calibration (CurveJacobianMode flag)** — optional analytic
  Jacobian mode for yield-curve calibration. See
  `docs/methodology/yield_curve_jacobian.md`.

## 2026-07

- `curve`: Unified passive and AAD curve construction across piecewise-constant forwards,
  piecewise-linear forwards, and log-discount curves. Linear and natural-cubic interpolation
  now separate passive geometry from typed ordinates, all log-DF schemes share one boundary
  and extrapolation implementation, and both single and joint calibration can use AAD-derived
  analytic Jacobians for every implemented representation. Joint declarations may mix methods
  and base-layer any implemented forward representation over an actively calibrated discount
  curve while preserving declaration-order solver columns. Newly analytic PWC/PWL
  `APPROXIMATE` solves can select a different tolerance-satisfying curve than the historical
  bumped path because the underdetermined solver stops at `fitTolerance_`; callers requiring
  historical curve-level reproduction must select `BUMPED`. `ZERO_RATE` remains unimplemented.
  See `docs/methodology/interpolation.md`, `docs/methodology/log_discount_curve.md`, and
  `docs/methodology/yield_curve_jacobian.md`.

- `numerics`: Corrected three output-affecting quantitative contracts: rate-aware
  Dupire now prices a discounted spot call and includes the strike in
  $(r-q)K C_K$; the exact underdetermined solver uses the quadratic model's
  $k=(c-b)/(a-2b+c)$ backtrack fraction; and Bachelier pricing/implied volatility
  now supports all real forward/strike pairs with a finite, nonnegative
  price-unit bracket and translation-invariant tolerances.
  See `docs/methodology/dupire.md`, `docs/methodology/underdetermined_search.md`,
  and `docs/methodology/black_scholes.md`.
- `runtime`: Made Monte Carlo reject non-positive path counts at the public boundary,
  made the DAL thread pool lazy and configurable with `DAL_NUM_THREADS`, and added
  size-safe batching with thread-local active AAD models and propagated task failures.
  Stopping the pool waits for work already claimed by a worker or caller and cancels
  work still queued. Native valuation now excludes concurrent evaluation-date mutation
  while allowing date getters to progress; the Python valuation and date bindings
  release the GIL around synchronized native work. Python `DoubleMatrix_` now supports
  rectangular nested-list construction and mutable indexing, enabling non-flat Dupire
  surfaces through Python and the web gateway.
- `build`: Added relocatable `DAL::cpp` / `DAL::public` CMake packages and an
  installed-consumer check, including exported MSVC runtime metadata and a helper for
  matching consumer targets; added `core-dev`, `full-dev`, and portable `distribution`
  profiles; moved the automated Linux install into `build/stage`; and made native-CPU
  tuning opt-in through `DAL_ENABLE_NATIVE_ARCH`.
- `web`: Defined the backend as native-only and added startup preflight checks that
  preserve and validate the locally installed `dal` package before Uvicorn starts.

- `curve`: Added opt-out controls for exact-calibration diagnostic matrix construction:
  `CurveCalibrationOptions_::computeEffJacobianInverse_`,
  `CurveCalibrationOptions_::computeForwardJacobian_`, and
  `JointMultiCurveCalibrationOptions_::computeJacobianAtSolution_`. Defaults preserve the existing
  diagnostics surface, while performance-sensitive callers can run solve-only calibrations. See
  `docs/methodology/yield_curve.md` and `docs/methodology/yield_curve_jacobian.md`.

- `pde`: Implemented the `Rollback_`-based PDE framework: coefficient factories and callable
  adapters, endpoint-exact concentrating coordinate maps, grid materialization, node-location
  derivative operators, and `ThetaScheme_` with explicit `Prepare`/decomposition reuse. The old
  mesher/`FD1D_` stack was removed, and `european_fd` plus `pde_perf` now use the new framework.
  See `docs/methodology/pde.md`. Breaking for direct `dal-cpp` PDE internals only; no
  `dal-public`/Python/Excel surface changed.

- `script`: The compiled (flat-stream) evaluator is now at strict capability
  parity with the tree-walk evaluators while `MCSimulation` keeps tree-walk as
  the default (`compiled=false`) in both specializations (`<double>` and
  `<AAD::Number_>`; `dal-cpp/dal/script/simulation.hpp`): fuzzy smoothing
  (call-spread/butterfly
  kernels shared via `dal-cpp/dal/script/visitor/smoothing.hpp`, dt-blend `FuzzyIf`,
  per-condition `eps` overrides, `maxNestedIfs`), const variables (live `ConstVar`
  opcode preserving const-var greeks), past events, and `NodeCollect_` all produce
  the same numbers (tol 1e-8) through either path. `ScriptProduct_::Compile(fuzzy)`
  is now `const` and returns a `ScriptCompiled_` artifact; `MCSimulation` compiles
  internally when requested, and `compiled` is `std::optional<bool>` (unset =
  tree-walk / `false`).
  Exposed through `ValueByMonteCarlo` (`dal-public/src/value.hpp`) and the Python
  `MonteCarlo_Value` binding as a backward-compatible `compiled` keyword. **Breaking
  API behavior**: (1) `AND`/`OR` are now eager in ALL evaluators — both
  operands always evaluate; scripts must not rely on short-circuit (condition
  expressions in this grammar are side-effect-free, so parseable scripts are
  unaffected); (2) the
  compiled evaluator remains opt-in with the same numbers and ~20-25% faster
  runtime in the benchmarked path; (3) `Compile()` signature/semantics changed
  from mutating member streams to a const artifact factory. See
  `docs/methodology/script_engine.md`.

- `random`: Sobol normal draws default to the fast Acklam inverse-CDF path;
  precise-CDF Newton correction is explicitly opt-in through `precise=true,
  polish=true` on the core, public C++, Python, and Excel constructors. Fast-CDF
  Newton polish (`precise=false, polish=true`) remains separately opt-in, and
  Sobol clones preserve sequence state and both policy flags. Pseudo-random
  normal draws retain their precise default. See `docs/methodology/random.md`.

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

<!-- Add new qualifying changes below as dated sections, e.g. -->
<!-- ## 2026-06 -->
<!-- - `curve`: Added log-linear interpolation to the interpolation module (non-breaking). -->
