# Core C++ Examples

Examples are grouped by their primary purpose. Every program uses core DAL APIs
and links `DAL::cpp` (or its legacy `dal_library` alias). Public-facade examples
have their own [guide](../../docs/examples.md#public-facade-and-owning-results) and build option.

## Products

Structured payoffs and their script representation, pricing and sensitivities.

| Program                                                  | Build target                  | Shows                                                                |
|----------------------------------------------------------|-------------------------------|----------------------------------------------------------------------|
| [average_option_script](products/average_option_script/) | `average_option_script`       | Arithmetic-average call: conditional quadrature, Monte Carlo and AAD |
| [basket_mc](products/basket_mc/)                         | `basket_mc`                   | Two-asset basket call and a conditional Black reference              |
| [digital](products/digital/)                             | `digital`                     | Digital option pricing and sensitivities                             |
| [script](products/script/)                               | `script`                      | Scripted product definition and evaluation                           |
| [script_settings](products/script_settings/)             | `dal_script_settings_example` | Contract settings, historical fixings and owning diagnostics         |
| [script_tree](products/script_tree/)                     | `script_tree`                 | Barrier-product syntax trees in Unicode and ASCII                    |
| [snowball](products/snowball/)                           | `snowball`                    | Snowball autocallable pricing and Greeks                             |
| [uoc](products/uoc/)                                     | `uoc`                         | Up-and-out call pricing and Greeks                                   |

## Curves

Yield-curve construction, interpolation, calibration and risk, including cross-currency workflows.

| Program                                                                | Build target                    | Shows                                                             |
|------------------------------------------------------------------------|---------------------------------|-------------------------------------------------------------------|
| [curve_calibration](curves/curve_calibration/)                         | `curve_calibration`             | Single- and multi-curve construction and instrument repricing     |
| [euribor3m_curve](curves/euribor3m_curve/)                             | `euribor3m_curve`               | Euribor 3M curve construction                                     |
| [interpolate_curve](curves/interpolate_curve/)                         | `interpolate_curve`             | Compare log-discount interpolation schemes                        |
| [joint_multi_curve_calibration](curves/joint_multi_curve_calibration/) | `joint_multi_curve_calibration` | Joint multi-curve calibration                                     |
| [quote_risk](curves/quote_risk/)                                       | `quote_risk`                    | Single-curve quote risk and coordinate reporting                  |
| [underdetermined](curves/underdetermined/)                             | `underdetermined`               | Yield-curve calibration with underdetermined search               |
| [xccy_curve_calibration](curves/xccy_curve_calibration/)               | `xccy_curve_calibration`        | Staged cross-currency basis calibration                           |
| [xccy_mtm_calibration](curves/xccy_mtm_calibration/)                   | `xccy_mtm_calibration`          | Joint domestic, foreign and resettable cross-currency calibration |
| [xccy_reset_pricing](curves/xccy_reset_pricing/)                       | `xccy_reset_pricing`            | Reset-aware cross-currency swap pricing                           |
| [yield_curve_jacobian](curves/yield_curve_jacobian/)                   | `yield_curve_jacobian`          | Compare analytic and finite-difference curve Jacobians            |

## Models

Complex model construction, calibration and pricing with GSR, SLV and hybrid composition.

| Program                                            | Build target                | Shows                                                            |
|----------------------------------------------------|-----------------------------|------------------------------------------------------------------|
| [gsr_slv_calibration](models/gsr_slv_calibration/) | `gsr_slv_calibration`       | Multi-factor GSR and stochastic-local-volatility calibration     |
| [gsr_swap_swaption](models/gsr_swap_swaption/)     | `gsr_swap_swaption`         | GSR swap and swaption pricing                                    |
| [hybrid_script](models/hybrid_script/)             | `dal_hybrid_script_example` | Hybrid model composition with scripted rate and exercise payoffs |

## Methods

Numerical methods and comparison results: Monte Carlo, PDEs, AAD, sampling and parallel execution. The Excel writer supports exporting numerical results.

| Program                                     | Build target      | Shows                                                                         |
|---------------------------------------------|-------------------|-------------------------------------------------------------------------------|
| [aad](methods/aad/)                         | `aad`             | Native AAD and analytic Black Greeks                                          |
| [aad_financial](methods/aad_financial/)     | `aad_financial`   | Weighted and blocked risk, shared portfolios, segmented MC and LSMC curvature |
| [aad_operators](methods/aad_operators/)     | `aad_operators`   | Recorded solves and roots, structural Jacobians and HVPs                      |
| [american_put_mc](methods/american_put_mc/) | `american_put_mc` | Compare LSMC early exercise with European and PDE references                  |
| [concurrency](methods/concurrency/)         | `concurrency`     | Compare serial and parallel execution                                         |
| [european_aad_fd](methods/european_aad_fd/) | `european_aad_fd` | Fixed-grid PDE risk with native AAD                                           |
| [european_fd](methods/european_fd/)         | `european_fd`     | Compare explicit, Crank-Nicolson and implicit PDE schemes                     |
| [european_mc](methods/european_mc/)         | `european_mc`     | Monte Carlo pricing and Greek estimators                                      |
| [excelwriter](methods/excelwriter/)         | `excelwriter`     | Export numerical results to Excel (MSVC and Office only)                      |
| [fix_mc](methods/fix_mc/)                   | `fix_mc`          | Monte Carlo scenarios with historical and simulated fixings                   |
| [sobol](methods/sobol/)                     | `sobol`           | Sobol quasi-random sampling                                                   |
| [vanilla](methods/vanilla/)                 | `vanilla`         | Compare analytic, Monte Carlo, finite-difference and AAD results              |

## Build and run

From the workspace root, enable core examples with the public component disabled:

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_BUILD_PUBLIC=OFF -DDAL_CPP_BUILD_EXAMPLES=ON
cmake --build build/Release-linux --target snowball curve_calibration gsr_slv_calibration european_fd -j 4
ctest --test-dir build/Release-linux -R '^(snowball|curve_calibration|gsr_slv_calibration|european_fd)$' --output-on-failure
```

Source directories follow `dal-cpp/examples/<category>/<program>/`. Target names,
CTest names and installed program names are independent of that source layout.
For example, the single-configuration Linux build places the Snowball executable
at `build/Release-linux/dal-cpp/examples/products/snowball/snowball`; installation
places it in `bin/snowball`. Multi-configuration generators also add the build
configuration, such as `Release`, to the executable directory. CTest resolves the
correct target location automatically.

`dal_script_settings_example` and `dal_hybrid_script_example` are build-only
targets; run them through their CTest entries.

`european_mc` keeps its `examples_slow` label. `excelwriter` requires MSVC and
Microsoft Office type libraries and is excluded from automatic smoke tests because
it starts a live Excel COM instance. The other examples keep the `examples` label;
UOC's output and invalid-argument checks remain registered with the UOC target.

Shared `aadreport.hpp` and `floatformat.hpp` stay at the examples root. Numerical
algorithms live in the library; the examples illustrate their use. See the
[native AAD workflows](../../docs/examples.md), [installation guide](../../docs/installation.md)
and [methodology index](../../docs/README.md#quantitative-methods) for details.
