# Numerical Quadrature

This note describes the one-dimensional quadrature rules exposed by
`dal-cpp/dal/math/integral/quadrature.hpp` (`Quadrature::NCDFGaussHermiteWeights`,
`Quadrature::SimpsonWeights`, `NormalExpectation_`, `QuadSimpson_`). The library
ships two rules for two distinct purposes:

- **Gauss-Hermite** for expectations under a standard normal,
  $\mathbb{E}[f(Z)]$ with $Z \sim \mathcal{N}(0,1)$.
- **Composite Simpson's $1/3$** for smooth integrals over a finite interval.

Both are wrapped by the `Quad1DFixed_<T_>` driver, which walks a precomputed
abscissa/weight pair and accumulates a weighted sum.

## Gauss-Hermite for Standard-Normal Expectations

### Why Gauss-Hermite

A Gauss-Hermite rule evaluates integrals of the form
$\int_{-\infty}^{\infty} g(x)\,e^{-x^2}\,dx$ exactly for polynomial $g$ of
degree up to $2n-1$, using $n$ nodes. To turn a standard-normal expectation
into that form, absorb the density into the kernel:

$$
\mathbb{E}[f(Z)] = \int_{-\infty}^{\infty} f(z)\,\frac{e^{-z^2/2}}{\sqrt{2\pi}}\,dz
                 = \frac{1}{\sqrt{\pi}} \int_{-\infty}^{\infty} f(\sqrt{2}\,x)\,e^{-x^2}\,dx .
$$

The last equality is the change of variable $z = \sqrt{2}\,x$. A Gauss-Hermite
rule with $n$ nodes applied to the right-hand integral therefore gives

$$
\mathbb{E}[f(Z)] \approx \sum_{i=1}^{n} w_i\,f(x_i),
$$

where $x_i$ are the Hermite nodes (in standard-normal coordinates) and $w_i$
are the Gauss-Hermite weights rescaled by the $1/\sqrt{\pi}$ factor. This is
the rule `NCDFGaussHermiteWeights` produces: it returns abscissa already in
$z$-space and weights already carrying the normal-density normalisation, so a
caller can evaluate $\mathbb{E}[f(Z)]$ by summing $w_i f(x_i)$ directly with no
further rescaling.

### Hermite Polynomial Recurrence

The nodes are the $n$ zeros of the physicist's Hermite polynomial $H_n$. Rather
than forming $H_n$ in monomial basis (catastrophically ill-conditioned for
large $n$), evaluation uses the orthonormal three-term recurrence

$$
\tilde{H}_0(x) = \pi^{-1/4}, \qquad \tilde{H}_1(x) = \sqrt{2}\,x\,\pi^{-1/4},
$$

$$
\tilde{H}_j(x) = x\sqrt{\tfrac{2}{j}}\,\tilde{H}_{j-1}(x) - \sqrt{\tfrac{j-1}{j}}\,\tilde{H}_{j-2}(x), \quad j \ge 2,
$$

which keeps $\{\tilde{H}_j\}$ orthonormal under $\int \tilde{H}_j \tilde{H}_k e^{-x^2} dx = \delta_{jk}$. The
constant $\pi^{-1/4}$ is $H_0$ in this normalisation and is folded into the
recurrence seed. This is why the construction never needs an explicit
coefficient array: each iteration promotes $(\tilde{H}_{j-2}, \tilde{H}_{j-1})$ to
$\tilde{H}_j$ in place.

### Newton Iteration on the Roots

The positive roots of $H_n$ are found one at a time by Newton's method. At a
trial point $x$, the recurrence gives $H_n(x)$, and the derivative uses the
Hermite identity $H_n'(x) = \sqrt{2n}\,H_{n-1}(x)$, so the Newton step is

$$
x \;\leftarrow\; x - \frac{H_n(x)}{H_n'(x)} = x - \frac{H_n(x)}{\sqrt{2n}\,H_{n-1}(x)} .
$$

Because $H_n$ is even or odd (matching the parity of $n$) and its roots are
symmetric about zero, only the positive roots are iterated for; each
positive root $r$ contributes the mirrored pair $\pm\sqrt{2}\,r$ to the
abscissa vector. Iteration stops when the step falls below a tight absolute
tolerance, and throws after a fixed iteration cap rather than looping
indefinitely on a divergent seed.

### Initial Root Guesses

Newton convergence is sensitive to the starting point, and the rightmost
roots of $H_n$ are widely spaced, so each root is seeded by a purpose-built
asymptotic or extrapolation formula rather than a uniform guess:

- The outermost root uses a classical asymptotic in $n$.
- Each subsequent root extrapolates from the previously found roots, using
  progressively simpler schemes as the roots cluster more tightly toward
  zero.

The extrapolation uses the *Hermite-space* roots (pre-$\sqrt{2}$ mapping), so
the recurrence-variable mirrors they reference are divided by $\sqrt{2}$
before reuse. Seeding close to a true root keeps every Newton iteration in
the basin of quadratic convergence, which is what makes the fixed iteration
cap safe.

### Weights and the Standard-Normal Mapping

Once a root $r$ of $H_n$ is known with derivative $H_n'(r)$, the corresponding
Gauss-Hermite weight is

$$
w = \frac{2}{\sqrt{\pi}\,\bigl[H_n'(r)\bigr]^2} .
$$

The factor $2/\sqrt{\pi}$ is the standard conversion from the bare
Gauss-Hermite weight $2/[H_n'(r)]^2$ to the standard-normal-expectation
weight (it is the $1/\sqrt{\pi}$ from the density substitution combined with
the $\sqrt{2}$ Jacobian folded into the abscissa). The node/weight pair is
written symmetrically into the output vectors:

$$
x_{\text{left}} = -\sqrt{2}\,r, \quad x_{\text{right}} = +\sqrt{2}\,r, \quad
w_{\text{left}} = w_{\text{right}} = w .
$$

The output abscissa are sorted ascending because positive roots are found in
descending order and mirrored as the iteration proceeds.

### Exactness and Moment Checks

Because the underlying rule is exact for polynomials of degree up to $2n-1$,
a `NormalExpectation_` with $n$ nodes integrates the standard-normal moments
$\mathbb{E}[Z^{2k}] = (2k-1)!!$ exactly when $2k \le 2n-1$, equivalently
$k \le n-1$. Odd moments vanish by the symmetric node/weight construction.
In particular a constant evaluates to $1$, the first moment to $0$, the variance
to $1$ with two nodes, and the fourth moment to $3$ with at least **three**
nodes. A two-node rule is exact only through degree three and therefore does not
integrate the fourth moment exactly.

## Composite Simpson Quadrature

`SimpsonWeights` builds the composite Simpson $1/3$ rule on $[lo, hi]$ from a
requested count $n$ of **grid points**. It first selects the actual odd point count
$N=n\mathbin{|}1$. Simpson's rule fits a parabola across each consecutive triple of
grid points, which gives the weight stencil

$$
\int_{x_0}^{x_{2}} f \approx \frac{h}{3}\bigl(f_0 + 4 f_1 + f_2\bigr),
$$

tiled across the interval. Endpoints carry coefficient $1$, odd-indexed
interior nodes $4$, even-indexed interior nodes $2$, every coefficient scaled
by $h/3$ where $h = (hi - lo)/(N-1)$.

### Odd Point Count

Simpson's $1/3$ rule requires an even number of subintervals, hence an odd
number of grid points, so the parabolic stencil tiles the interval without
remainder. The constructor therefore forces the requested point count odd with
a bitwise OR (`n | 1`). A caller passing an even $n$ gets $n+1$ points rather
than a parity error. With the resulting point count $N$, the spacing is
$h=(hi-lo)/(N-1)$.

The result is exact for cubic polynomials and has global error $O(h^4)$ for a
sufficiently smooth integrand. Halving $h$ therefore reduces the leading error
by approximately a factor of $16$.

## The `Quad1DFixed_` Driver

Both rules share a single accumulator, `Quad1DFixed_<T_>`, which holds the
abscissa $x$, the weights $w$, a running sum, and a cursor. The caller drives
it through a pull-style loop:

1. `GetX()` returns the current abscissa.
2. The caller evaluates the integrand at that point and hands it back via
   `PutY(y)`, which adds $w_i \cdot y$ to the accumulator and advances the
   cursor.
3. `IsComplete()` reports when all nodes have been consumed.
4. `Result()` returns the accumulated sum.

The accumulator is templated on the integrand's value type $T_$. The default
scalar specialisation accumulates $w \cdot y$ directly. The `Vector_<>`
specialisation uses `Transform` with `LinearIncrement_`, so a single
`Quad1DFixed_<Vector_<>>` integrates a vector-valued integrand
component-wise in one pass. `Restart()` rewinds the cursor and resets the
sum, allowing the same node set to be reused across several integrands.

### Convenience Wrappers

`NormalExpectation_<T_>` is a `Quad1DFixed_<T_>` whose constructor fills
`x_` and `w_` with `NCDFGaussHermiteWeights`. It is the canonical way to
compute an expectation under a standard normal; for a non-unit variance,
the caller composes the integrand as $f(\mu + \sigma z)$ at each `GetX()` and
multiplies the final result by $\sigma$ only when the integrand is a
density-style rescaling — for plain payoff expectations the affine map in $z$
is sufficient.

`QuadSimpson_<T_>` is the analogue for finite-interval integration: its
constructor takes the bounds and node count and fills `x_` and `w_` with
`SimpsonWeights`.

## When to Use Which

| Rule                     | Use for                                          | Notes                                                              |
|--------------------------|--------------------------------------------------|--------------------------------------------------------------------|
| `NormalExpectation_<T_>` | $\mathbb{E}[f(Z)]$ over a standard normal        | Spectral accuracy for smooth $f$; $n$ nodes exact to degree $2n-1$ |
| `QuadSimpson_<T_>`       | $\int_{lo}^{hi} f(x)\,dx$ over a finite interval | Fourth-order globally; requested point count is forced odd         |

For expectations under a normal, Gauss-Hermite is strongly preferred over
Simpson on a truncated interval: it places nodes where the density has mass,
needs no truncation, and achieves exactness for polynomial payoffs that
Simpson matches only approximately and at far higher cost.

## Examples

No dedicated example program exercises the quadrature layer; the snippets below
are drawn from `dal-cpp/dal/math/integral/quadrature.hpp`. Both rules are driven
through the same pull-style loop on `Quad1DFixed_<T_>`.

`NormalExpectation_` evaluates a standard-normal expectation with a Gauss-Hermite
node set; $n$ nodes are exact for polynomial payoffs up to degree $2n-1$:

```cpp
// from dal-cpp/dal/math/integral/quadrature.hpp
#include <dal/math/integral/quadrature.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;

NormalExpectation_<> quad(/*n=*/8);   // 8 Gauss-Hermite nodes
while (!quad.IsComplete()) {
    const double z = quad.GetX();
    const double payoff = std::max(std::exp(z) - strike, 0.0);   // f(z)
    quad.PutY(payoff);
}
const double price = quad.Result();   // E[f(Z)], Z ~ N(0,1)
```

`QuadSimpson_` integrates a smooth integrand over a finite interval with the
composite $1/3$ rule; the requested point count is forced odd internally:

```cpp
// from dal-cpp/dal/math/integral/quadrature.hpp
QuadSimpson_<> quad(/*n=*/101, /*lo=*/0.0, /*hi=*/1.0);
while (!quad.IsComplete()) {
    const double x = quad.GetX();
    quad.PutY(std::sin(x));
}
const double integral = quad.Result();   // int_0^1 sin(x) dx
```

`Restart()` rewinds the cursor and resets the accumulator so the same node set
can be reused across several integrands without rebuilding the weights.

## See Also

- [Black / Bachelier vanilla pricing](black_scholes.md) — the normal CDF/PDF at the
  heart of the closed forms is the distribution `NormalExpectation_` integrates
  against when a payoff is not available in closed form.
- [Script engine](script_engine.md) — the Monte Carlo driver integrates payoffs
  pathwise rather than by quadrature, but `NormalExpectation_` is the right tool
  for low-dimensional analytic expectations that arise during model setup.
