# Native AAD Examples

These programs use DAL's built-in native AAD. Start with the workflow below;
each link points to executable source or a complete worksheet recipe. The
[installation guide](installation.md#native-aad-configuration) owns build options,
the [C++ facade guide](public-api.md) owns public entry points, and the
[Python](python/README.md) and [Excel](excel/README.md) guides own language input
and result contracts. Numerical definitions remain in the linked methodology.

## C++

Configure the workspace and build only the three new example targets:

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

[`aad_operators.cpp`](../dal-cpp/examples/aad_operators/aad_operators.cpp) demonstrates:

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

[`aad_financial.cpp`](../dal-cpp/examples/aad_financial/aad_financial.cpp) uses native
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

Existing examples cover [Black analytic Greeks](../dal-cpp/examples/aad/aad.cpp),
[complete fixed-grid European PDE risk](../dal-cpp/examples/european_aad_fd/european_aad_fd.cpp),
[curve Jacobians](../dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp)
and [first-order quote risk](../dal-cpp/examples/quote_risk/quote_risk.cpp).
PDE derivative accuracy and grid convergence are separate checks; see
[the PDE guide](methodology/pde/aad.md).

### Public facade and owning results

The independent [`aad_public_risk.cpp`](../dal-public/examples/aad_public_risk/aad_public_risk.cpp)
uses facade builders and `ValueByMonteCarloWithWeightedRisk`. It requests the
input order Y, X, attaches different reporting factors, releases the caller's
product/model handles and then reads the owning value, raw/report Jacobians,
axes and execution snapshot. Analytic checks distinguish coordinate order from
reporting units. This example complements the core preparation/batch workflows.

## Python

After installing or building `dal`, run from the repository root:

```bash
python dal-python/examples/017.native_aad_risk.py
python dal-python/examples/018.quote_curvature.py
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

Both numbered programs run under the existing Python example smoke-test harness.
They use no test fixtures or Python objective callbacks. C++ recording scopes,
generic equation callbacks and the smooth prototype remain C++ interfaces.

## Excel

Follow the [segmented MC and LSMC worksheet recipe](../dal-excel/examples/011.native_aad_risk.md).
It gives exact cell locations, input matrices, handle dependencies and result
queries. The [Excel interface](excel/README.md) documents all function signatures,
logical empty shapes and cell admission. Worksheet recipes require a loaded XLL;
they are not Linux execution evidence.
