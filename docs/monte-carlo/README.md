# Monte Carlo Methods

DAL's C++ script engine can price ordinary future cashflows and `EXERCISE`
products under Black-Scholes or Dupire model data. The public facade is shown
in [C++ API](../public-api.md#scripted-monte-carlo); binding-specific examples
live in [Python](../python/README.md) and [Excel](../excel/README.md).

1. [Simulation and evaluation](simulation.md) — path preparation, model
   observations, payments, batching, tree/compiled evaluation, and AAD.
2. [Sampling and path construction](sampling.md) — Sobol, Brownian bridge,
   pseudorandom generators, normal transforms, and path seeking.
3. [Least-squares Monte Carlo](lsm.md) — separate regression and pricing paths,
   backward induction, basis and solver, visitor pruning, exercise AAD, and
   policy diagnostics.
4. [Special techniques](special-techniques.md) — randomized QMC replicates,
   variance and sensitivity interpretation, held-out selection, and
   performance choices.
5. [Correlated equity Black-Scholes](correlated-bs.md) — core multi-asset
   simulation, named observations, factor ordering, and AAD parameters.
6. [Hybrid model](hybrid-model.md) — typed component composition, named-factor
   correlations, deterministic numeraire, and factor-aware Brownian bridge.

The [script language and preparation](../methodology/script_engine.md) guide
documents syntax, FIX history, AST passes, and diagnostic JSON.
