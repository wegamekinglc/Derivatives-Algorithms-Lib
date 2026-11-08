# PDE Methods

The C++ PDE layer provides one-dimensional grids, finite-difference operators,
and a `double` theta rollback. Start with the [framework](framework.md), then
see [European option pricing](option-pricing.md) for the runnable C++ example.
[Bermudan and American pricing](early-exercise.md) provides owning deterministic-
carry Black–Scholes requests, prices, exercise regions and failure diagnostics
through core C++, the public facade, Python and Excel. The separate Bermudan
helper used to validate [LSM](../monte-carlo/lsm.md) remains test support.

The separate [owning core risk request](early-exercise.md#owning-core-risk-request)
uses fixed-plan full repricing for Delta, finite-scale Gamma, Vega and native
carry partials, with explicit per-risk availability and cumulative budgets.
Risk projections to the public facade, Python and Excel remain planned;
numerical qualification covers fixed fixtures, bump scales and refinement levels.

[Fixed-grid European prices and adjoints](aad.md) demonstrates complete native
AAD rollback with terminal/boundary risks and separate derivative/convergence
acceptance.
