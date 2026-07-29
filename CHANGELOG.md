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
- **Cross-Currency Pricing and Calibration** — fixed, resettable, and mark-to-market
  swap pricing with immutable timestamped rate/FX fixing snapshots, staged basis
  fitting, simultaneous domestic/foreign/basis calibration, and named joint
  parameter/residual ranges. See `docs/methodology/xccy_calibration.md`.
- **Interpolation** — linear, log-linear, cubic-spline, and mixed 1D interpolators plus
  bilinear 2D interpolation. See `docs/methodology/interpolation.md`.
- **Log-Discount Curve** — node log-discount-factor parameterisation with `LogDfScheme_`
  interpolation schemes and scalar-generic passive/AAD evaluation. See
  `docs/methodology/log_discount_curve.md`.
- **Yield-Curve Jacobian and Inverse-Jacobian Risk** — AAD forward Jacobians for every
  implemented curve representation subject to the normal eligibility gates, plus the
  inverse-Jacobian IR-risk transform and its `effJacobianInverse_` unit convention. See
  `docs/methodology/yield_curve_jacobian.md`.
- **Script Engine** — events-table to AST pipeline, visitor passes (domain analysis,
  constant-condition folding), and the fuzzy evaluator for pathwise AAD through
  discontinuous payoffs. See `docs/methodology/script_engine.md`.

## 2026-07

- `curve`: Added immutable native rate-cashflow planning and pricing for
  `DEPOSIT`, `FRA`, `FUTURE`, `OIS`, `IRS`, `BASIS_SWAP`, and `XCCY`,
  including explicit historical rate/FX fixing demand, snapshot admission,
  passive and AAD valuation, and first-order node sensitivities. The same
  typed batch surface is additive in public C++ and Python. See
  `docs/methodology/yield_curve.md`,
  `docs/methodology/yield_curve_jacobian.md`, `docs/public-api.md`, and
  `docs/curve-lab.md`.

- `web`: Added the Curve Lab DAL-WEB workflow for visual seven-family
  authoring, immutable asynchronous build/import/risk runs, native
  `Storable_` JSON and `Bag_` version persistence, dependency/fixing
  provenance, exact quote axes, PV/DV01/KRD results, and replayable
  sensitivity matrices. See `docs/curve-lab.md` and `dal-web/README.md`.

- `matrix`: Added exact scaled-`alpha` candidate combination for `Sparse::CGSolve`
  and `Sparse::BCGSolve`. When the standalone binary64 coefficient is unsafe, the
  stored quotient remains exact until each complete solution, residual, or BCG
  shadow-residual expression is rounded once; finite cancellation and subnormal
  candidates are accepted, while genuinely non-finite candidates still fail before
  the atomic commit. The existing `beta/betaPrev` direction-ratio path is unchanged,
  and solver-level FTZ validation is limited to the S3/S5 first-iteration cases.
  Public signatures and bindings are unchanged. See `docs/methodology/matrix.md`.

- `matrix`: Made `Sparse::CGSolve` and `Sparse::BCGSolve` scale-safe across the
  finite binary64 range. Norm and convergence classification no longer relies
  on overflowed or underflowed intermediates, and ambiguous signed dot products
  use exact accumulation. Candidate updates are committed only after callback
  validation; candidates that appear converged additionally require direct
  residual confirmation. Public signatures and bindings are unchanged; extreme
  finite systems that previously reported false convergence or avoidable
  breakdown now solve or fail closed. See `docs/methodology/matrix.md`.

- `curve`: Added staged XCCY sensitivity diagnostics across public C++, Python,
  and Excel. The additive options overload selects analytic or bumped
  Jacobians and independently controls the forward and effective-inverse
  matrices while preserving the one-argument defaults. Diagnostics retain the
  instrument and basis-knot axes plus explicit availability, tolerance, and
  scaling metadata; the effective inverse is `solver_scaled`, so raw decimal
  quote bumps map as `dx = E * dq / tolerance`. Public C++ and Excel also expose
  the retained joint XCCY effective inverse. See
  `docs/methodology/xccy_calibration.md`,
  `docs/methodology/yield_curve_jacobian.md`, and `docs/public-api.md`.

- `curve`: Made calibration settings dictionaries strict on the Python and Excel
  surfaces. `dal.calibrate_curve` raises `ValueError` on an unknown settings key,
  and the Excel single-curve, staged-XCCY, and joint-XCCY settings parsers throw
  via `RequireKnownSettingsKey`; both name the offending key and the accepted set.
  Unknown keys were previously ignored silently, so a misspelled key now fails
  loudly instead of calibrating with defaults. Correct usage is unaffected.

- `core`: Migrated owning factory returns from raw pointers to `std::unique_ptr`
  across the `dal-cpp` core headers: the interpolation factories
  (`Interp::NewLinear`/`NewLinear2`/`NewLogLinear`/`NewCubic`, `NewMixedLogDF`),
  the PDE coordinate-map, coefficient, and derivative-operator factories, the
  random generators (`Random_::Clone`, `PseudoRandom_::Branch`/`New`,
  `SequenceSet_::TakeAway`, `NewSobol`), the direct curve factories
  (`NewDiscountPWC`/`ZeroRate`/`PWLF`/`LogDF`, `YCComponent_::Clone`,
  `BuildCurveCalibrationWeights`), the sparse decompositions, the matrix-writer
  helpers, the index parsers, `Underdetermined::Function_::Gradient`,
  `ModelData_::MutantModel`, and `Environment_`/`Composite_` iteration and
  cloning. `Handle_` gained a converting constructor from `std::unique_ptr`.
  **Breaking** for direct consumers of the `dal-cpp` core headers — code holding
  raw results or calling `.reset()`/`.release()` must now take the `unique_ptr`;
  `dal-public`, Python, and Excel signatures are unchanged. The
  Machinist-generated `Archive::Reader_::Build()` interface keeps its raw return
  and migrates in a follow-up.
- `matrix`: Fixed the `Matrix_` move constructor to value-initialize `cols_` —
  a moved-from matrix previously carried an indeterminate column count (latent
  UB present since 2022) and now has defined zero dimensions.

- `aad`: Fixed multi-result adjoint propagation on the native backend. Reverse
  sweeps now dispatch to the vector-adjoint path (`TapNode_::PropagateAll`)
  whenever `SetNumResultsForAAD(true, m)` is active; previously every sweep took
  the scalar `PropagateOne` path, so all $m$ result adjoints silently stayed
  zero. Consumed multi-mode adjoints are zeroed after propagation — the same
  discipline `PropagateOne` already applied to scalar adjoints — so repeated
  sweeps no longer re-propagate stale slots, and the multi-mode state
  (`Tape_::multi_`, `Tape_::numAdj_`) moved from process-global statics to
  per-tape members so tapes configured with different result arities no longer
  interfere. The `SetNumResultsForAAD` scope guard now restores the enclosing
  mode on destruction instead of forcing scalar defaults. The Adept, XAD, and
  CoDiPack backends are unaffected. See `docs/methodology/aad.md`.

- `time`: Made `Date_` construction strict. The year must lie in [1900, 2199],
  the month in [1, 12], and the day within the actual length of the given month
  (leap years included). Invalid triples such as `Date_(2023, 2, 30)` previously
  normalized silently through serial-date arithmetic; they now throw
  `Exception_` via `REQUIRE`. **Breaking behavior change** visible identically
  through C++, the Python `dal.Date_` binding, and all date-taking Excel
  functions.

- `curve`: Added reset-aware cross-currency pricing and simultaneous domestic,
  foreign, and basis calibration. Fixed, resettable, and mark-to-market notional
  modes replace the prior boolean configuration; explicit rate/FX fixing identities
  and one immutable timestamped snapshot support already-started swaps; and the
  joint solver exposes named parameter/residual ranges plus analytic or bumped
  Jacobians. Public C++ and joint Python expose both retained joint matrices. Excel
  exposes the joint forward Jacobian and ranges, but has no worksheet getter for the
  effective inverse. **Breaking:** the two
  `CrossCurrencyConvention_` booleans `resettableNotional_` and
  `markToMarketNotional_` are replaced by the enum in
  `CrossCurrencySwapConfig_`. The legacy fixed-notional convenience constructor
  and builder remain compatible. See `docs/methodology/xccy_calibration.md`,
  `docs/methodology/yield_curve_jacobian.md`, and `docs/public-api.md`.

- `curve`: Added persistent continuously compounded `ZERO_RATE` curves. Future-node
  rates map to `logDF = -z * YearFrac(anchor,node)` and reuse all shared log-DF
  interpolation/extrapolation schemes; the anchor has no free zero-rate parameter.
  Single, staged, and joint calibration support ZERO_RATE with passive or active base
  layering and AAD analytical Jacobians in future-node zero-rate order. The additive
  `DiscountZeroRate_v1` archive preserves representation and bump coordinates, and direct
  factories are available in core C++, public C++, Python (`DiscountZeroRate_New`), and
  Excel (`DISCOUNTZERORATE.NEW`). See `docs/methodology/yield_curve.md`,
  `docs/methodology/yield_curve_jacobian.md`, and `docs/public-api.md`.

- `curve`: Unified passive and AAD curve construction across piecewise-constant forwards,
  piecewise-linear forwards, and log-discount curves. Linear and natural-cubic interpolation
  now separate passive geometry from typed ordinates, all log-DF schemes share one boundary
  and extrapolation implementation, and both single and joint calibration can use AAD-derived
  analytic Jacobians for every implemented representation. Joint declarations may mix methods
  and base-layer any implemented forward representation over an actively calibrated discount
  curve while preserving declaration-order solver columns. Newly analytic PWC/PWL
  `APPROXIMATE` solves can select a different tolerance-satisfying curve than the historical
  bumped path because the underdetermined solver stops at `fitTolerance_`; callers requiring
  historical curve-level reproduction must select `BUMPED`. At that point, `ZERO_RATE` was
  deliberately outside the unified factory; the later entry above adds it without changing
  the other representation contracts.
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
- `web`: Added persistent single-curve, staged-XCCY, and joint-XCCY calibration
  APIs and the Curve Lab workbench. Versioned run and instrument records plus
  reconstructible curve rows preserve inputs, execution evidence, fit and matrix
  diagnostics, FX forwards, and the effective inverse without persisting native
  handles. Base curves are referenced by ID and recursively expanded on read;
  quote-bump previews are calculated per GET request from the persisted effective
  inverse and are not stored. Completed results survive a database-backed restart;
  orphaned running calibrations become failed on startup. See `dal-web/README.md`.
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
