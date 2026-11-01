# Autocallable examples

Ten executable examples share a monthly script builder and compare compiled
Monte Carlo prices with smoothed AAD prices and named input derivatives. They
link only `DAL::cpp`. Matching [Python examples](../../../../dal-python/examples/autocallables/)
and [Excel workbooks](../../../../dal-excel/examples/autocallables/) use the same
contract definitions and model inputs.

## Contracts

All examples have notional 1, valuation date 2026-01-02, twelve monthly
observations on the 2nd and maturity 2027-01-02. Cashflows include principal
redemption; the initial purchase price is excluded. Accrual is ACT/365F, without
business-day adjustment or payment lag. Rates and volatilities are decimal inputs.

| Source                                                     | Structure                                                                                                                 |
|------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [classic_snowball.cpp](classic_snowball.cpp)               | 12% annual accrued coupon; monthly 100% KO and 65% KI                                                                     |
| [stepdown_snowball.cpp](stepdown_snowball.cpp)             | Snowball KO starts at 105%, falls 1.5 percentage points each month, with an 85% floor                                     |
| [lockout_snowball.cpp](lockout_snowball.cpp)               | Snowball with no KO during the first five observations; KI monitoring starts in month one                                 |
| [phoenix.cpp](phoenix.cpp)                                 | Monthly conditional coupon, 12% annual rate, 80% coupon barrier; terminal 65% KI                                          |
| [memory_phoenix.cpp](memory_phoenix.cpp)                   | Missed Phoenix coupons accumulate and are paid at the next qualifying observation                                         |
| [stepdown_memory_phoenix.cpp](stepdown_memory_phoenix.cpp) | Memory coupons with the same descending KO schedule as the step-down snowball                                             |
| [airbag_snowball.cpp](airbag_snowball.cpp)                 | After KI, maturity redemption is `min(final performance / 0.65, 1)`                                                       |
| [capital_protected.cpp](capital_protected.cpp)             | Full principal redemption with an unconditional 4% annual coupon paid monthly until KO                                    |
| [upside_participation.cpp](upside_participation.cpp)       | Snowball with 6% annual coupon, 120% KO and 100% participation in positive terminal performance above 100% if still alive |
| [worst_of_phoenix.cpp](worst_of_phoenix.cpp)               | Phoenix on `min(A / 100, B / 120)` with two correlated Black–Scholes equities                                             |

KO triggers at or above its barrier, including maturity. Snowball KI triggers
strictly below 65% at any monthly observation. KO redeems principal plus accrued
coupon even after KI. Without KO, a snowball pays principal plus its annual
coupon if never knocked in; after KI it pays `min(final performance, 1)` with
no coupon. The airbag changes that downside redemption. The upside example adds
its terminal participation to the surviving snowball redemption.

Phoenix pays its qualifying monthly coupon before any same-date KO redemption.
Without KO it redeems principal unless final performance is below 65%, in which
case redemption equals final performance. Earlier KI breaches do not affect
Phoenix redemption. Unpaid memory coupons are forfeited at maturity. The
capital-protected example always returns principal at KO or maturity.

These definitions specify illustrative discrete contracts, including the
particular airbag and participation conventions. Capital protection describes
the scripted cashflow; issuer default and funding spreads are not modeled.

## Model and risk

Single-asset examples use normalized spot 1, volatility 20%, rate 2% and dividend
yield 0. The worst-of example uses spots 100 and 120, volatilities 20% and 25%,
correlation 40%, rate 2% and zero dividend yields. Initial reference levels stay
fixed when model spots change.

Both valuations use the same Sobol paths, without Brownian bridge. The double
price uses hard barriers; AAD uses smoothing width 0.001 in normalized
performance units. Their prices can differ. Derivatives describe the smoothed
contract with contract terms fixed, and do not propagate through calibration.
Spot derivatives use the corresponding model's spot units; rate, volatility and
dividend derivatives are per unit decimal input. No sampling error is reported
from this single deterministic Sobol run.

## Build and run

From the repository root, a core-only build demonstrates component independence:

```bash
cmake --preset=Release-linux -S . -B build/autocallables-core -DDAL_BUILD_PUBLIC=OFF -DDAL_CPP_BUILD_TESTS=OFF
cmake --build build/autocallables-core --target autocallables -j 4
build/autocallables-core/dal-cpp/examples/products/autocallables/autocallable_memory_phoenix 16384
ctest --test-dir build/autocallables-core -R '^autocallable_' --output-on-failure
```

Each target and installed executable is named `autocallable_<source stem>`.
The `autocallables` aggregate builds all ten. The optional positional argument
sets the positive path count; the default is 16384. CTest runs each example with
1024 paths under the `examples` label. These smoke checks exercise both pricing
modes and require finite prices and derivatives. Existing library algorithms
are unchanged, so algorithm benchmark reruns are outside this example change.
