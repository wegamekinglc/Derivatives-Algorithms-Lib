# Methodology

These guides describe the algorithms and conventions used by DAL's current
C++ implementation. Model and curve-specific mathematics lives in the
[models](../models/README.md), [yield curves](../yield-curves/README.md), and
[currency curves](../ccy-curves/README.md) chapters.

## Pricing Methods

- [Monte Carlo](monte-carlo/README.md) — simulation, sampling and path
  construction, least-squares exercise pricing, randomized QMC, and performance
  choices.
- [PDE](pde/README.md) — one-dimensional grids, finite-difference operators,
  theta rollback, European pricing/adjoints, and deterministic-carry
  Bermudan/American pricing.

## Evaluation and Differentiation

- [Automatic adjoint differentiation](aad.md) — tape ownership, recording,
  derivatives, parallel evaluation, and curve-calibration primitives.
- [Structural Jacobians](aad-sparsity.md) — conservative supports, compressed
  native execution, and complete rate-trade Jacobians.
- [Scoped passive constants](passive-constants.md) — core GSR Monte Carlo opt-in,
  recording ownership, and memory limits.
- [Script engine](script_engine.md) — syntax, preparation, FIX observations,
  evaluator passes, and diagnostics.
- [Core script calibration contracts](script_calibration_contract.md) — immutable
  contract data, typed bindings, quote/observation axes and persistence.

## Numerical Routines

- [Interpolation](interpolation.md).
- [Matrix and linear algebra](matrix.md).
- [Quadrature](quadrature.md).
- [Underdetermined search](underdetermined_search.md).

## Market Conventions

- [Dates, calendars, and schedules](dates.md).
- [Index names and parsing](index_parsing.md).
