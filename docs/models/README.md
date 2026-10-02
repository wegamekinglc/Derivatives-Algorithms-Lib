# Models

These guides describe DAL's option-pricing models and their simulation data.
The Black/Bachelier guide also covers the closed-form kernels used as vanilla
benchmarks. For path generation, regression, and Monte Carlo evaluation, see
[Monte Carlo methods](../methodology/monte-carlo/README.md).

1. [Black-Scholes and Bachelier](black-scholes.md) — lognormal and normal
   vanilla pricing, implied volatility, and Black-Scholes model examples.
2. [Dupire surface calibration](dupire.md) — implied-volatility inversion and
   calibration grid for reusable local-volatility data.
3. [Correlated equity Black-Scholes](correlated-bs.md) — named multi-asset
   observations, constant correlations, and AAD parameters.
4. [Hybrid Monte Carlo](hybrid-model.md) — named equity, constant or term-structure
   deterministic rates, factor correlation, and model risks.
5. [Gaussian Short Rate (GSR)](gaussian-short-rate.md) — named Gaussian factors,
   dated OIS and projection curves, stochastic discounting, rate observations, swaps,
   swaptions, and Bermudan exercise.
6. [Local volatility in hybrid models](local-volatility.md) — reusable and
   serializable local-vol grids with BS or stochastic GSR rates.
