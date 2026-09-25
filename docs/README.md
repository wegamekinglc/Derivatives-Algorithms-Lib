# DAL Documentation

These guides describe the current repository implementation. API and
methodology changes are recorded in [CHANGELOG.md](../CHANGELOG.md). Except in
the dedicated Python and Excel chapters, examples use C++ by default.

## Start Here

- [Installation](installation.md) — build profiles, staged installs, and package setup.
- [Architecture](architecture.md) — component boundaries and valuation flows.
- [C++ public API](public-api.md) — facade headers, construction, pricing, and risk.
- [Python interface](python/README.md) — bindings, settings, examples, and package usage.
- [Excel interface](excel/README.md) — worksheet functions, handles, and examples.
- [Contributing](../CONTRIBUTING.md) — build, test, and review workflow.

## Quantitative Methods

| Chapter | Contents |
|---------|----------|
| [Yield curves](yield-curves/README.md) | Construction, log-discount representation, calibration Jacobians, node and quote risk |
| [CCY curves](ccy-curves/README.md) | Cross-currency pricing, fixing snapshots, staged and joint calibration |
| [Monte Carlo](monte-carlo/README.md) | Simulation, sampling, LSM exercise pricing, RQMC, AAD, and performance |
| [PDE](pde/README.md) | One-dimensional theta rollback and European option example |

The remaining C++ methodology guides cover [AAD](methodology/aad.md),
[Black/Bachelier pricing](methodology/black_scholes.md),
[Dupire local volatility](methodology/dupire.md),
[script syntax and preparation](methodology/script_engine.md),
[interpolation](methodology/interpolation.md),
[matrix algorithms](methodology/matrix.md),
[quadrature](methodology/quadrature.md),
[underdetermined search](methodology/underdetermined_search.md),
[dates and calendars](methodology/dates.md), and
[index parsing](methodology/index_parsing.md).

## Component Guides

- [Core C++](../dal-cpp/README.md) and [public C++ facade](../dal-public/README.md).
- [Python package](../dal-python/README.md) and [Excel add-in](../dal-excel/README.md).
- [Excel FIX settings](excel/script-settings.md) — worksheet matrices, dates,
  snapshots, diagnostics, and executable workbook.

The [experimental studies](experimental/) are reference explorations rather
than supported methodology.

## Documentation Conventions

Use relative links between guides and repo-relative paths when naming source
files. Mathematical notation uses `$...$` and `$$...$$`. Put reusable method
explanations in the corresponding chapter and update this index when adding a
chapter. Source comments should retain local implementation constraints.
