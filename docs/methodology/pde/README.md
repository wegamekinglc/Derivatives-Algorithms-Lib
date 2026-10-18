# PDE Methods

The C++ PDE layer provides one-dimensional grids, finite-difference operators,
and a `double` theta rollback. Start with the [framework](framework.md), then
see [European option pricing](option-pricing.md) for the runnable C++ example.
[Bermudan and American pricing](early-exercise.md) provides owning deterministic-
carry Black–Scholes requests, prices, exercise regions and failure diagnostics
through core C++, the public facade, Python and Excel. The separate Bermudan
helper used to validate [LSM](../monte-carlo/lsm.md) remains test support.

[Fixed-grid European prices and adjoints](aad.md) demonstrates complete native
AAD rollback with terminal/boundary risks and separate derivative/convergence
acceptance.
