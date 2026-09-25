# Yield Curves

These guides describe the C++ yield-curve implementation and its risk surfaces.
Examples use C++ unless a guide explicitly points to the dedicated
[Python](../python/README.md) or [Excel](../excel/README.md) interface chapter.

1. [Construction and calibration](construction.md) — discount and forward-curve
   representations, instrument equations, sequential and joint calibration.
2. [Log-discount curve](log-discount.md) — node coordinates, interpolation,
   extrapolation, and serialization.
3. [Jacobian and inverse-Jacobian risk](jacobian-risk.md) — calibration derivatives,
   parameter-to-quote risk, and XCCY matrix layouts.
4. [Rate-trade node risk](node-risk.md) — AAD eligibility and batch risk cells.
5. [Joint quote risk](joint-quote-risk.md) — coupled curve/base risk and
   provenance.

The underlying solver is documented in
[underdetermined search](../methodology/underdetermined_search.md); the
cross-currency application has its own [CCY curves](../ccy-curves/README.md)
chapter.
