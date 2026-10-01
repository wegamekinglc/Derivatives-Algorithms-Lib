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

| Chapter                                | Contents                                                                              |
|----------------------------------------|---------------------------------------------------------------------------------------|
| [Models](models/README.md)             | Black-Scholes, GSR, Hybrid, local volatility, and Dupire calibration                  |
| [Yield curves](yield-curves/README.md) | Construction, log-discount representation, calibration Jacobians, node and quote risk |
| [CCY curves](ccy-curves/README.md)     | Cross-currency pricing, fixing snapshots, staged and joint calibration                |
| [Methodology](methodology/README.md)   | Monte Carlo, PDE, AAD, script evaluation, numerical routines, and market conventions  |

## Component Guides

- [Core C++](../dal-cpp/README.md) and [public C++ facade](../dal-public/README.md).
- [Python package](../dal-python/README.md) and [Excel add-in](../dal-excel/README.md).
- [Excel FIX settings](excel/script-settings.md) — worksheet matrices, dates,
  snapshots, diagnostics, and executable workbook.

The [PTIRDS replication note](experimental/replicate-ptirds-single-currency-curve.md)
compares supported curve behavior with an external benchmark.

## Documentation Conventions

Use relative links between guides and repo-relative paths when naming source
files. Mathematical notation uses `$...$` and `$$...$$`. Put reusable method
explanations in the corresponding chapter and update this index when adding a
chapter. Use one canonical page per topic and link to it directly; remove obsolete
redirect pages when reorganizing guides. Favor readable explanations and useful
examples over exhaustive detail. Source comments should stay brief and explain
local constraints; references run from documentation to code, not from code to
documentation.
