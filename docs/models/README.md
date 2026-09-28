# Models

These guides describe DAL's option-pricing models and their simulation data.
The Black/Bachelier guide also covers the closed-form kernels used as vanilla
benchmarks. For path generation, regression, and Monte Carlo evaluation, see
[Monte Carlo methods](../monte-carlo/README.md).

1. [Black-Scholes and Bachelier](black-scholes.md) — lognormal and normal
   vanilla pricing, implied volatility, and Black-Scholes model examples.
2. [Dupire local volatility](dupire.md) — implied-volatility inversion,
   calibration grid, and local-volatility simulation.
3. [Correlated equity Black-Scholes](correlated-bs.md) — named multi-asset
   observations, constant correlations, and AAD parameters.
4. [Hybrid Monte Carlo](hybrid-model.md) — named equity, constant or term-structure
   deterministic rates, factor correlation, and model risks.
5. [Gaussian Short Rate (GSR)](gaussian-short-rate.md) — dated OIS and
   projection curves, stochastic discounting, rate observations, swaps,
   swaptions, and Bermudan exercise.
