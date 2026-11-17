# Market F/D workflow

From the repository root:

```bash
cmake --preset Release-linux -S . -B build/market-core -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF
cmake --build build/market-core --target dal_market_iv_local_vol_example -j 4
ctest --test-dir build/market-core -R '^dal_market_iv_local_vol_example$' --output-on-failure
```

This core-only program fits twelve decimal-IV quotes with eight positive F/D
terms, builds a 21×13 spot-major Dupire grid, and constructs an owning local-vol
model. It prints calibration residuals, rho/eta/certification, source/model
identities, vanilla/forward/DF prices and preparation counters. All products
use an explicit valuation date. Horizon is 1.25 years; max step is 1/52 year.
Warnings identify the first-column extension and both tail policies.

The forward and unit-payment references are independent F×D and D values at
one year. The call comparison uses the fitted analytic surface and a broad
workflow guard. These small runs do not replace grid/time/statistical
qualification. Ordinary passive pricing is supported; AAD/risk, EXERCISE,
model archives and pickle are unavailable. See the
[model contract](../../../../docs/models/local-volatility.md#owning-market-model-in-core-c).

The separate [public program](../../../../dal-public/examples/market_iv_local_vol/)
and [Python script](../../../../dal-python/examples/022.market_iv_local_vol.py)
use the same market construction. The [Excel recipe](../../../../dal-excel/examples/014.market_iv_local_vol.md)
uses a compact three-term constant-IV variant.
