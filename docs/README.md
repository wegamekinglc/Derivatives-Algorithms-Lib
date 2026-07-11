# DAL Documentation

This directory contains technical documentation for the Derivatives Algorithms Library (DAL).

The library is documented as a single current version: the docs here always describe the latest
state. Historical context — breaking changes, new methodologies, and significant capability
additions — is recorded in the repo-root [CHANGELOG.md](../CHANGELOG.md).

## Start Here

- **[Installation guide](installation.md)** — prerequisites, build profiles, staged
  installs, Python bindings, and web startup
- **[Architecture](architecture.md)** — component boundaries, runtime ownership,
  generated code, valuation, and calibration flows
- **[Public API guide](public-api.md)** — supported C++, Python, and Excel entry points
- **[Contributing](../CONTRIBUTING.md)** — build, test, generation, formatting, docs,
  and review expectations

## Component Guides

- **[Repository overview](../README.md)** — workspace entry point and examples
- **[Core C++](../dal-cpp/README.md)** — quantitative engine, tests, and examples
- **[Public C++ facade](../dal-public/README.md)** — convenience API and compatibility contract
- **[Python bindings](../dal-python/README.md)** — package usage and Python API
- **[Excel add-in](../dal-excel/README.md)** — Windows XLL and worksheet functions
- **[Web application](../dal-web/README.md)** — FastAPI/React application workflow

## Installation

- **[installation.md](installation.md)** — Complete Installation Guide
  - System requirements (C++ compiler, Python, Node.js)
  - C++ library installation (Linux and Windows)
  - Python bindings setup with uv
  - Web UI installation and startup
  - Verification and troubleshooting

### Methodology (`methodology/`)

Deep dives into the quantitative methods and algorithms implemented in DAL:

- **[aad.md](methodology/aad.md)** — Automatic Adjoint Differentiation (AAD)
  - Expression templates, tape management, reverse-mode propagation
  - Backend architecture (native, XAD, CoDiPack, Adept)
  - Parallel AAD for Monte Carlo simulations
  - Tape-layer curve calibration primitives (`DiscountPWLF_`, `JointCurveBlock_`, `JointRate_`)

- **[yield_curve.md](methodology/yield_curve.md)** — Yield Curve Construction
  - Discount curve framework (`DiscountPWLF_`, `DiscountPWC_`)
  - Piecewise-linear and piecewise-constant forward rates
  - Multi-curve construction and calibration (sequential and joint simultaneous)
  - Single-curve AAD calibration internals (TapeGuard, eligibility predicate, analytic Jacobian)
  - Joint simultaneous calibration (`CalibrateJointMultiCurve`) with stacked-parameter AAD Jacobian
  - Joint vs staged calibration drift characteristics
  - Integration with the underdetermined solver

- **[underdetermined_search.md](methodology/underdetermined_search.md)** — Underdetermined Optimization
  - Scaled quasi-Newton method for underdetermined systems
  - Residual scaling, quadratic backtrack fraction, forward Jacobian capture at solution
  - Solver controls structure and Broyden update regime
  - Application to yield curve calibration via smoothness penalties

- **[xccy_calibration.md](methodology/xccy_calibration.md)** — Cross-Currency Calibration
  - Cross-currency market and basis curve framework
  - Cross-currency swap pricing and conventions
  - Multi-instrument term structure calibration
  - Integration with the underdetermined solver

- **[interpolation.md](methodology/interpolation.md)** — Interpolation
  - Linear, log-linear, cubic-spline, and mixed one-dimensional interpolators
  - `MIXED` compatibility orientation: linear head and natural-cubic tail
  - Cubic boundary conditions (`Boundary_` order/value)
  - Bilinear (2D) interpolation on a rectilinear grid
  - Selection guidance and where each scheme is used

- **[matrix.md](methodology/matrix.md)** — Matrix and Linear Algebra
  - Numerical-Recipes band-storage layout and the `Sparse::Square_` / decomposition interfaces
  - Tri-diagonal Thomas-algorithm solve (`TriDiagonal_`, `TriDecomp_`, `TriDecompSymm_`)
  - Dense and band-Cholesky factorizations with diagonal regularization
  - Preconditioned conjugate-gradient (CG) and bi-conjugate-gradient (BCG) Krylov solvers

- **[log_discount_curve.md](methodology/log_discount_curve.md)** — Log-Discount Curve
  - Node log-discount-factor representation and anchor convention
  - `LogDfScheme_` interpolation schemes (`LOG_LINEAR`, `LOG_CUBIC_NATURAL`, `MIXED`)
  - Thomas algorithm for the natural-cubic system, basis weights, and `fppCoef_` matrix
  - Serialization version design (v1 without scheme, v2 with named scheme)
  - Why `LOG_DISCOUNT` is the parameterization that supports the analytic Jacobian

- **[pde.md](methodology/pde.md)** — PDE Framework
  - Coordinate maps, including identity, sinh, and endpoint-exact concentrating maps
  - `CoordinateVector_`, `GridLocations`, and uniform/concentrating grid builders
  - Node-location-based tridiagonal derivative operators and boundary-row convention
  - Coefficient factories and callable adapters for scalar/vector/matrix coefficients
  - `ThetaScheme_` rollback, explicit `Prepare`, decomposition reuse, and value-layer layout

- **[yield_curve_jacobian.md](methodology/yield_curve_jacobian.md)** — Yield-Curve Jacobian and Inverse-Jacobian Risk
  - Forward residual Jacobian via AAD reverse sweep vs finite-difference bump
  - Inverse-Jacobian IR-risk transform `r = gᵀ · effJacobianInverse_ / tolerance_`
  - Why `effJacobianInverse_` carries an extra `tolerance_` factor (solver residual scaling)

- **[script_engine.md](methodology/script_engine.md)** — Script Engine
  - Preprocessing pipeline (macros, schedules, constant variables)
  - Domain processor (variable range analysis, always-true/false flags)
  - Constant condition processor (dead-branch pruning)
  - Fuzzy evaluator (smooth transitions for pathwise AAD; nested-if merging)

- **[dupire.md](methodology/dupire.md)** — Dupire Local Volatility
  - Discounted spot-call contract and rate-aware Dupire formula
  - Central-difference IVS inversion (`IVS_::LocalVol`) and relative bump sizing
  - Calibration grid construction (`DupireCalib`, `DupireCalibMaturity`)
  - 2.5-$\Sigma$ strike cutoff, the spot-strike-call $\sqrt{2\pi}$ proxy, and flat-tail extrapolation

- **[black_scholes.md](methodology/black_scholes.md)** — Black / Bachelier Vanilla Pricing
  - Black (lognormal) and Bachelier (normal) European closed forms, de-annualized vol convention
  - Bachelier pricing for all real forward/strike pairs and finite nonnegative implied-vol bracketing
  - Forward delta and vega greeks by `OptionType_` (`CALL` / `PUT` / `STRADDLE`)
  - `DistributionNormalLike_` shared base, vega-notional (`VolVega`), parameter derivatives
  - Translation-invariant Bachelier tolerances, finite-input checks, and intrinsic floor

- **[quadrature.md](methodology/quadrature.md)** — Numerical Quadrature
  - Gauss-Hermite construction (orthonormal Hermite recurrence, Newton root search, node/weight mapping)
  - Standard-normal-expectation rule `NCDFGaussHermiteWeights` / `NormalExpectation_`
  - Composite Simpson's 1/3 rule, odd-point forcing, and global fourth-order convergence
  - The `Quad1DFixed_<T_>` pull-style driver loop and vector-valued integration

- **[random.md](methodology/random.md)** — Random Number Generation and Path Construction
  - `Random_` interface and the pseudo-random vs. quasi-random split
  - Brownian bridge: bisection order, conditional mean/variance, variation normalization
  - Sobol direction numbers and the Gray-code $O(1)$ recurrence
  - Sobol inverse-CDF policy table and clone-equivalent state/flag preservation
  - Path seeking via direct state reconstruction (`SobolSet_::SkipTo`, MRG32k32a matrix jump)

### Experimental (`experimental/`)

Reference studies and capability explorations that are not normative methodology:

- **[aad-analytic-jacobian-curve-calibration.md](experimental/aad-analytic-jacobian-curve-calibration.md)**
  — Compatibility redirect to the supported yield-curve and AAD methodology.
- **[replicate-ptirds-single-currency-curve.md](experimental/replicate-ptirds-single-currency-curve.md)**
  — Validated rateslib/PTIRDS single-currency curve replication.

## Documentation Conventions

All documentation uses GitHub-flavored Markdown with:

- **Mathematical notation** — LaTeX-style math in `$...$` (inline) or `$$...$$` (display)
- **Code references** — Inline code with backticks, file paths relative to repo root
- **Cross-references** — Links between docs use relative paths (e.g., `[AAD](methodology/aad.md)`)

## Contributing

Follow the repository [contributor guide](../CONTRIBUTING.md). When adding documentation:

1. **Methodology docs** go in `docs/methodology/` — explain algorithms, math, and design decisions
2. **Update this index** — add a brief description and link to new documents
3. **Cross-reference** — link related documents using relative paths

Keep documentation focused and technical. Docs own the **WHY** (methodology, math, invariants); source comments own the **WHAT** (local intent, invariants that must live next to the code they constrain). When a comment grows into methodology prose, move the prose here and reduce the comment to a short pointer rather than duplicating it in both places.
