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
  - Joint multi-curve AAD analytic Jacobian (reverse-sweep, backend-neutral)
  - Integration with the underdetermined solver

- **[underdetermined_search.md](methodology/underdetermined_search.md)** — Underdetermined Optimization
  - Scaled quasi-Newton method for underdetermined systems
  - Application to yield curve calibration
  - Regularization and smoothness penalties

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

- **[log_discount_curve.md](methodology/log_discount_curve.md)** — Log-Discount Curve
  - Node log-discount-factor representation and anchor convention
  - `LogDfScheme_` interpolation schemes (`LOG_LINEAR`, `LOG_CUBIC_NATURAL`, `MIXED`)
  - Why `LOG_DISCOUNT` is the parameterization that supports the analytic Jacobian

- **[yield_curve_jacobian.md](methodology/yield_curve_jacobian.md)** — Yield-Curve Jacobian and Inverse-Jacobian Risk
  - Forward residual Jacobian via AAD reverse sweep vs finite-difference bump
  - Inverse-Jacobian IR-risk transform `r = gᵀ · effJacobianInverse_ / tolerance_`
  - Why `effJacobianInverse_` carries an extra `tolerance_` factor (solver residual scaling)

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

Keep documentation focused and technical. Avoid duplicating information that belongs in code comments or the main README.
