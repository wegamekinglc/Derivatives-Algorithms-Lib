# DAL Documentation

This directory contains technical documentation for the Derivatives Algorithms Library (DAL).

The library is documented as a single current version: the docs here always describe the latest
state. Historical context — breaking changes, new methodologies, and significant capability
additions — is recorded in the repo-root [CHANGELOG.md](../CHANGELOG.md).

## Documentation Structure

### Installation

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
  - Joint multi-curve AAD analytic Jacobian (reverse-sweep, backend-neutral)
  - Integration with the underdetermined solver

- **[underdetermined_search.md](methodology/underdetermined_search.md)** — Underdetermined Optimization
  - Scaled quasi-Newton method for underdetermined systems
  - Residual scaling, backtracking line search, forward Jacobian capture at solution
  - Solver controls structure and Broyden update regime
  - Application to yield curve calibration via smoothness penalties

- **[xccy_calibration.md](methodology/xccy_calibration.md)** — Cross-Currency Calibration
  - Cross-currency market and basis curve framework
  - Cross-currency swap pricing and conventions
  - Multi-instrument term structure calibration
  - Integration with the underdetermined solver

- **[interpolation.md](methodology/interpolation.md)** — Interpolation
  - Linear, log-linear, cubic-spline, and mixed one-dimensional interpolators
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

- **[pde.md](methodology/pde.md)** — PDE Finite-Difference Meshers and Coordinate Maps
  - `FDM1DMesher_` interface, `locations_` / `dplus_` / `dminus_`, and the boundary-null convention
  - `Uniform1DMesher_` constant spacing and endpoint pinning
  - `Concentrating1dMesher_` sinh/asinh coordinate stretch, density scaling, and snapped-knot device
  - `CoordinateMap_`, `NewSinhMap(xWidth, dxdyRange)`, and identity degeneration

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
  - Dupire local-volatility formula from an IVS
  - Central-difference IVS inversion (`IVS_::LocalVol`) and relative bump sizing
  - Calibration grid construction (`DupireCalib`, `DupireCalibMaturity`)
  - 2.5-$\Sigma$ strike cutoff, the ATM-call $\sqrt{2\pi}$ proxy, and flat-tail extrapolation

- **[black_scholes.md](methodology/black_scholes.md)** — Black / Bachelier Vanilla Pricing
  - Black (lognormal) and Bachelier (normal) European closed forms, de-annualized vol convention
  - Forward delta and vega greeks by `OptionType_` (`CALL` / `PUT` / `STRADDLE`)
  - `DistributionNormalLike_` shared base, vega-notional (`VolVega`), parameter derivatives
  - Brent implied-vol inversion (`BlackIV` / `BachelierIV`) and intrinsic floor

### Experimental (`experimental/`)

Notes on capabilities that are working but not yet promoted to normative methodology. These
may change shape before becoming methodology docs:

- **[aad-analytic-jacobian-curve-calibration.md](experimental/aad-analytic-jacobian-curve-calibration.md)**
  — AAD-derived analytic Jacobian for yield-curve calibration, gated by the runtime
  `CurveJacobianMode_` flag (`BUMPED` default / `ANALYTIC` opt-in) on
  `CurveCalibrationOptions_`. Backend-neutral across all four AAD backends.
- **[replicate-ptirds-single-currency-curve.md](experimental/replicate-ptirds-single-currency-curve.md)**
  — Replication study for single-currency-curve PTI ratchet/digital swaps.

## Documentation Conventions

All documentation uses GitHub-flavored Markdown with:

- **Mathematical notation** — LaTeX-style math in `$...$` (inline) or `$$...$$` (display)
- **Code references** — Inline code with backticks, file paths relative to repo root
- **Cross-references** — Links between docs use relative paths (e.g., `[AAD](methodology/aad.md)`)

## Relationship to `.claude/`

The `.claude/` directory contains **operational configuration** for the Claude Code agent:

- `.claude/rules/` — Style guides and coding conventions enforced by the agent
- `.claude/agents/` — Agent definitions (orchestrator, implementer, reviewer, etc.)
- `.claude/skills/` — Reusable skills and workflows

These are **not reference documentation** but rather instructions for AI-assisted development. See them in the repository root.

## Contributing

When adding new documentation:

1. **Methodology docs** go in `docs/methodology/` — explain algorithms, math, and design decisions
2. **Update this index** — add a brief description and link to new documents
3. **Cross-reference** — link related documents using relative paths

Keep documentation focused and technical. Docs own the **WHY** (methodology, math, invariants); source comments own the **WHAT** (local intent, invariants that must live next to the code they constrain). When a comment grows into methodology prose, move the prose here and reduce the comment to a short pointer rather than duplicating it in both places.
