# Finite-Difference Option Pricing

The public C++ PDE module is a one-dimensional `double` rollback framework.
The runnable [European call example](../../dal-cpp/examples/european_fd/european_fd.cpp)
constructs a Black-Scholes operator and compares explicit, Crank–Nicolson,
and fully implicit theta schemes with the Black closed form.

## Equation and Scheme

For spot $S$, rate $r$, dividend yield $q$, and volatility $\sigma$, the
example supplies discounting $r$, advection $(r-q)S$, and diffusion
$\sigma^2 S^2$. `ThetaScheme_(theta)` represents a backward step for the
resulting spatial operator: `theta = 0` is explicit, `0.5` is
Crank–Nicolson, and `1` is fully implicit. Explicit stepping needs a finer
time grid for stability; Crank–Nicolson and implicit stepping solve a
tridiagonal system. See the [framework](framework.md#theta-scheme) for the
operator construction and decomposition reuse.

The call payoff is $\max(S-K,0)$ at maturity. At each new rollback level the
example seeds the left boundary with zero and the right boundary with
$S_{\max}e^{-q\tau}-Ke^{-r\tau}$. These are target-level boundary values;
they belong in a separate output layer before `ThetaScheme_::operator()`.
The source and output layers are swapped after the step.

## C++ Rollback Pattern

The following excerpt follows `european_fd.cpp`; `x`, `grids`, `vals`, and
`next` are the C++ grid and value layers constructed in that example:

```cpp
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/thetascheme.hpp>

const Dal::Handle_<Dal::PDE::ScalarCoeff_> disc(Dal::PDE::NewConstCoeff(rate));
const Dal::Handle_<Dal::PDE::VectorCoeff_> mu(
    Dal::PDE::NewVectorCoeff([=](double s) { return (rate - div) * s; }));
const Dal::Handle_<Dal::PDE::MatrixCoeff_> var(
    Dal::PDE::NewMatrixCoeff([=](double s) { return vol * vol * s * s; }));
Dal::PDE::ThetaScheme_ scheme(0.5);
scheme.Prepare(dt, grids, *disc, *mu, *var);
for (int n = 0; n < timeSteps; ++n) {
    (*next[0])(0, 0, 0) = 0.0;
    (*next[0])(0, 0, numX - 1) =
        maxX * std::exp(-div * (n + 1) * dt) -
        strike * std::exp(-rate * (n + 1) * dt);
    scheme(dt, grids, vals, *disc, *mu, *var, &next);
    vals.Swap(&next);
}
```

`Prepare` can reuse a decomposition while the grid, step, and probed
coefficients remain the same. The example interpolates the final value layer
at spot with a cubic interpolator and prints differences from the Black
analytic price over a Crank–Nicolson grid-refinement sweep.

## Early Exercise Boundary

`Rollback_` and `ThetaScheme_` do not implement obstacle projection, PSOR,
or penalty methods. The repository's
[Bermudan PDE helper](../../dal-cpp/test-support/bermudan_pde.hpp) is test-only:
it rolls back between exercise dates and projects onto the put payoff at
those dates to validate [LSM pricing](../monte-carlo/lsm.md). The
[American put C++ example](../../dal-cpp/examples/american_put_mc/american_put_mc.cpp)
also applies an obstacle after each rollback step locally; that code is an
example benchmark, not a public PDE pricer. The production early-exercise
method is the script engine's LSM driver.
