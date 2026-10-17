# PDE Methods

The C++ PDE layer provides one-dimensional grids, finite-difference operators,
and a `double` theta rollback. Start with the [framework](framework.md), then
see [European option pricing](option-pricing.md) for the runnable C++ example
and the boundary of the public API. The Bermudan obstacle rollback used to
validate Monte Carlo is test support only; see [LSM](../monte-carlo/lsm.md).

[Fixed-grid European prices and adjoints](aad.md) demonstrates complete native
AAD rollback with terminal/boundary risks and separate derivative/convergence
acceptance.
