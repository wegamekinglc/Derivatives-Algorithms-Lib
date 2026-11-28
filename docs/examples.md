# Native AAD Examples

The matching [C++](../dal-cpp/examples/products/autocallables/README.md),
[Python](../dal-python/examples/autocallables/README.md) and
[Excel](../dal-excel/examples/autocallables/README.md) autocallable collections
cover ten contract structures with double prices and smoothed AAD derivatives.

These programs use DAL's built-in native AAD. Start with the workflow below;
each link points to executable source or a complete worksheet recipe. The
[installation guide](installation.md#native-aad-configuration) owns build options,
the [C++ facade guide](public-api.md) owns public entry points, and the
[Python](python/README.md) and [Excel](excel/README.md) guides own language input
and result contracts. Numerical definitions remain in the linked methodology.

## C++

The [core C++ catalog](../dal-cpp/examples/README.md) covers products, curves,
models and numerical methods. The native AAD operator and financial workflows
below are in `dal-cpp/examples/methods/`.

Configure the workspace and build these three example targets:

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_BUILD_PUBLIC=ON -DDAL_PUBLIC_BUILD_EXAMPLES=ON
cmake --build build/Release-linux --target aad_operators aad_financial aad_public_risk -j 4
ctest --test-dir build/Release-linux -R '^(aad_operators|aad_financial|aad_public_risk)$' --output-on-failure
```

`aad_operators` and `aad_financial` live under `dal-cpp/examples/` and link only
`DAL::cpp`; they build without the public component. `aad_public_risk` is a
separate program under `dal-public/examples/` and links `DAL::public` when that
component and its examples are enabled. All are installed into `bin/`, take no
input files, and are registered as CTest `examples` smoke tests.

### Recorded operators and curvature

[`aad_operators.cpp`](../dal-cpp/examples/methods/aad_operators/aad_operators.cpp) demonstrates:

- A checked dense linear solve composed with a scalar objective; its reverse
  report remains readable after recording close.
- An implicit square-root equation with an explicit converged candidate and
  residual policy. The operator differentiates the equation at that candidate.
- A 3-by-4 structural Jacobian recovered from two compressed directions using
  declared conservative supports.
- A signed quadratic HVP computed by both finite-step bump-over-AAD and the
  opt-in smooth forward-over-reverse prototype.

Every displayed derivative is checked against an independent analytic reference.
See [recorded solves and roots](methodology/aad.md#recorded-dense-linear-solves),
[structural supports](methodology/aad-sparsity.md),
[finite-step products](methodology/aad.md#directional-curvature-with-bump-over-aad)
and [the smooth prototype](methodology/aad.md#smooth-native-forward-over-reverse-prototype).
The prototype uses its own active type; general native `higherOrder_` remains false.

### Financial risk and common paths

[`aad_financial.cpp`](../dal-cpp/examples/methods/aad_financial/aad_financial.cpp) uses native
preparation and batch APIs to demonstrate:

- Ordered component outputs, a Jacobian in blocks of at most two output lanes and weighted VJP,
  checked against both a polynomial reference and the weighted Jacobian rows.
- A two-trade portfolio sharing one original model handle; preparation groups
  the trades and generates one shared scenario per path.
- A segmented Black–Scholes request with absolute path offset 7 and a spot
  curvature direction and two payment dates. Quadratic payoffs check Delta and Gamma against exact
  identities on those same realized paths, independently of sampling error.
- A two-date Bermudan put under both Frozen and RetrainedBump, displaying PV,
  spot Delta and directional curvature with separate inner and outer steps.

The 33 MC paths and 129 pricing / 256 training LSMC paths keep the example small.
They illustrate the interfaces, not converged prices or an unbiased stopping
Gamma. Assess sampling dispersion, outer-step bias and inner-policy error
separately; see [weighted risk](methodology/aad.md#weighted-script-risk-results),
[portfolio ownership](methodology/aad.md#sealed-script-portfolio-coordinates),
[segmented MC curvature](methodology/aad.md#common-path-segmented-monte-carlo-curvature)
and [LSMC estimator semantics](methodology/aad.md#native-lsmc-policy-curvature).

Existing examples cover [Black analytic Greeks](../dal-cpp/examples/methods/aad/aad.cpp),
[complete fixed-grid European PDE risk](../dal-cpp/examples/methods/european_aad_fd/european_aad_fd.cpp),
[curve Jacobians](../dal-cpp/examples/curves/yield_curve_jacobian/yield_curve_jacobian.cpp)
and [first-order quote risk](../dal-cpp/examples/curves/quote_risk/quote_risk.cpp).
PDE derivative accuracy and grid convergence are separate checks; see
[the PDE guide](methodology/pde/aad.md).

### Pointwise second-order AAD and Dupire quote Greeks

[`aad_second_order.cpp`](../dal-cpp/examples/methods/aad_second_order/aad_second_order.cpp)
uses `AAD::EvaluateSecondOrder` on a European Black–Scholes call with spot 103
and decimal volatility 0.24. The direction matrix has columns for spot and
volatility. Its two basis rows give Gamma, Vanna and Volga, and the signed row
`(-2, 0.5)` gives $g^T v$ and $Hv$. The program compares the price, gradient and
Hessian products with analytic Greeks.

[`dupire_quote_second_order.cpp`](../dal-cpp/examples/models/dupire_quote_second_order/dupire_quote_second_order.cpp)
builds the six-quote snapshot and prepares the registered one-year call struck
at 105. It passes the plan's exact quote axis to `EvaluateDupireQuoteSecondOrder`.
A single `quote:3` basis direction uses pointwise AAD. A three-row request adds
the `quote:2` basis direction and a mixed row and uses shared AAD. The table
prints PV, six quote Vegas and the three HVP columns in plan order. In the
`quote:3` basis HVP, entries `quote:3`, `quote:2` and `quote:1` give Volga,
cross-tenor and cross-strike curvature, respectively. Quote spreads are decimal
volatility units. Use round-to-nearest with gradual underflow as described in
the [fixed-plan contract](models/dupire.md#fixed-plan-european-call-quote-greeks).

Both programs link `DAL::cpp`, require no input files and are registered under
the CTest `examples` label. From the workspace root, initialize the dependencies
and run a Release build with only the core component enabled:

```bash
git submodule update --init dal-cpp/externals/rapidjson dal-cpp/externals/machinist
cmake -S . -B build/core-second-order -DCMAKE_BUILD_TYPE=Release \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_CPP_BUILD_TESTS=OFF \
  -DDAL_CPP_BUILD_BENCHMARKS=OFF -DDAL_CPP_BUILD_EXAMPLES=ON -DDAL_USE_EIGEN=OFF
cmake --build build/core-second-order --config Release --target aad_second_order dupire_quote_second_order --parallel 4
ctest --test-dir build/core-second-order -C Release --output-on-failure \
  -R '^(aad_second_order|dupire_quote_second_order)$' -V
```

### Public facade and owning results

The independent [`aad_public_risk.cpp`](../dal-public/examples/aad_public_risk/aad_public_risk.cpp)
uses facade builders and `ValueByMonteCarloWithWeightedRisk`. It requests the
input order Y, X, attaches different reporting factors, releases the caller's
product/model handles and then reads the owning value, raw/report Jacobians,
axes and execution snapshot. Analytic checks distinguish coordinate order from
reporting units. This example complements the core preparation/batch workflows.

[`script_curve_quote_risk.cpp`](../dal-public/examples/script_curve_quote_risk/script_curve_quote_risk.cpp)
calibrates two Deposit quotes, captures explicit accuracy in an owning snapshot
and values the original fixed-coupon contract. It checks snapshot and per-PV
availability, displays signed USD PV/raw/DV01 in requested quote order and reads
a live-quote rejection after the inputs leave scope. See the
[script curve risk contract](methodology/script_calibration_contract.md#script-curve-quote-risk-snapshots).

The core [compounded_ois](../dal-cpp/examples/curves/compounded_ois/compounded_ois.cpp)
uses only `DAL::cpp`, including with public disabled. The separate facade
[compounded_ois_quote_risk](../dal-public/examples/compounded_ois_quote_risk/compounded_ois_quote_risk.cpp)
links `DAL::public` and is enabled by `DAL_PUBLIC_BUILD_EXAMPLES`. Both show
four `/2` modes, same-source EXACT calibration, fixed K/N and full/PV-only
signed quote DV01 against complete quote recalibration. The legacy facade
[compounded_ois_script](../dal-public/examples/compounded_ois_script/compounded_ois_script.cpp)
retains its `/1` mixed-history/direct-native example. Target, CTest and installed
executable names match these program names; installed examples are in `bin/`.
See the [OIS calling guide](ccy-curves/compounded-ois.md) and
[installed core/public consumers](installation.md#installed-ois-consumers).

## Python

After installing or building `dal`, run from the repository root:

```bash
python dal-python/examples/017.native_aad_risk.py
python dal-python/examples/018.quote_curvature.py
python dal-python/examples/020.compounded_ois_quote_risk.py
```

[`017.native_aad_risk.py`](../dal-python/examples/017.native_aad_risk.py) mirrors the
financial C++ workflows and adds a call/put fixed-grid PDE result with the complete
rate/volatility/strike Jacobian. It prints the actual spot and raw derivative units.

[`018.quote_curvature.py`](../dal-python/examples/018.quote_curvature.py) builds an
exact two-deposit calibration, prices an off-knot deposit and checks recalibrated
quote gradients/products against an independent log-discount formula. It also
builds a native Dupire/Hybrid plan with a direct quote binding and checks its
quote Gamma against a closed-form payoff with zero calibrated-surface partial.
A second payoff includes `FIX(EQ[LOCAL])`, so its common-path estimate includes
both the calibrated surface and direct quote response. That 17-path example
prints the estimate without claiming a converged Gamma or an analytic oracle.

Quote directions use complete raw decimal quote axes. HVPs are finite-step
products; reported Gamma/cross-Gamma requires both coordinate scaling factors.
The [rate](methodology/aad.md#native-rate-trade-quote-curvature) and
[Dupire](methodology/aad.md#common-path-c-monte-carlo-quote-curvature) methodology
defines the recalibration boundary and estimator limits.

The two AAD/curvature programs run under the existing Python example smoke-test harness.
They use no test fixtures or Python objective callbacks. C++ recording scopes,
generic equation callbacks and the smooth prototype remain C++ interfaces.

[`020.compounded_ois_quote_risk.py`](../dal-python/examples/020.compounded_ois_quote_risk.py)
shows the four OIS modes, owning audit, EXACT calibration and fixed-contract
full/PV-only signed DV01. Its checks also execute under `python -O`; it
distinguishes raw quote derivatives from native curve partials and retains an
owning unavailable diagnostic. A matching native module is required.

## Excel

The [OIS worksheet contract](excel/README.md#explicit-compounded-ois) and
[calling recipe](ccy-curves/compounded-ois.md#excel-calling-recipe) give typed
convention/preparation handles and the prepared/evaluated 22/26-column audits.
Portable binding checks do not execute native XLL/OPER registration or Office
COM/spill; those require separate Windows/Excel evidence.

Follow the [segmented MC and LSMC worksheet recipe](../dal-excel/examples/011.native_aad_risk.md).
It gives exact cell locations, input matrices, handle dependencies and result
queries. The [Excel interface](excel/README.md) documents all function signatures,
logical empty shapes and cell admission. Worksheet recipes require a loaded XLL;
they are not Linux execution evidence.
