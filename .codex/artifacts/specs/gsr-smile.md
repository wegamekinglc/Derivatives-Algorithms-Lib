# GSR and rate smile implementation

Active scope: implement the approved multi-factor GSR, Heston-style rate
stochastic local volatility, calibration, and public C++/Python/Excel surfaces.
SABR-style volatility and equity/FX composition are subsequent extensions.

## Delivery order

1. Multi-factor Gaussian rates, shared curve/observation kernel, compatibility,
   archives, named factors, and public bindings.
2. European rate-option pricing and bounded, regularized calibration of Gaussian
   volatility buckets with fixed factor loadings and correlations.
3. Markovian HJM extension with curve state x, symmetric accumulated covariance Y,
   and a normalized CIR variance process. Local leverage changes diffusion and
   the HJM drift together. Gaussian limits must reproduce stage 1.
4. Stochastic-volatility/local-leverage calibration, independent validation,
   calibration diagnostics, and calibrated quote sensitivities.

## Gaussian contract

All factor g/H functions are dated, left-continuous-in-interval piecewise constants
(the value at a knot is the new segment). Brownian factor names preserve input
order; correlation is finite, symmetric, unit-diagonal and positive semidefinite.
Zero and singular covariance are supported. New factor H values may be signed;
the legacy one-factor object's strictly positive H contract stays intact.

With C(t)=diag(g(t)) R diag(g(t)), V(t)=integral(0,t) C(u) du,
B(t,T)=integral(t,T) H(u) du, and
m(t)=-integral(0,t) C(u) B(u,t) du, the bond price is
P0(T)/P0(t) exp[-B'(S-m)-B'VB/2]. Endpoint state transitions are exact Gaussian.
Pure-rate event payoffs use the conditional expectation of each interval's
continuous discount factor. A singular covariance must not receive an artificial
diagonal volatility floor.

Legacy factories, archives, risk labels, RNG ordering and single-factor Hybrid
behavior remain compatible. New model input risks use factor-qualified g/H labels.
Correlation is initially passive; no correlation AAD risk is claimed.

## Smile contract

Under the domestic risk-neutral measure, dx=Y H dt+Sigma dW and
dY=Sigma Sigma' dt, with x(0)=Y(0)=0. The bond price is
P0(T)/P0(t) exp[-B'x-B'YB/2]. Sigma includes positive local leverage and sqrt(v).
The CIR process has v(0)=1, removing its scale degeneracy with g. All correlated
drivers must form a PSD matrix. Discounting uses the simulated bank account;
the Gaussian endpoint discount shortcut is not presumed valid for smile dynamics.

## Calibration contract

Quotes have explicit expiry, underlying schedule, strike, settlement, value and
tolerance. Fit prices with meaningful error scales. Gaussian g buckets are fitted
with H/R fixed first. Non-Gaussian parameters and local leverage are released in
separate passes with bounds, smoothing and common random numbers. Validation uses
independent paths and smaller steps. Report failed convergence, residuals, parameter
bounds, numerical error and conditioning. A single leverage cannot be advertised as
an exact fit to an arbitrary multi-tenor cube.

Model-input AAD and calibrated quote risk are separate. Quote risk must include
curve provenance and the derivative of the actual regularized calibration optimum.

## Acceptance

- Legacy one-factor paths, observations and AAD agree with existing behavior.
- Initial discount curves reprice at nodes; projection behavior is unchanged.
- Two/three-factor covariances, bond prices and discount moments match independent
  analytic calculations, including changing g/H knots and rank deficiency.
- Brownian bridge dimensions, clone parameter ownership, archive round trips,
  invalid inputs and public binding parity have focused tests.
- The smile model has the Gaussian limit and numerically nonnegative variance;
  discounted-bond expectations converge as the simulation grid is refined.
- Synthetic calibration, held-out prices and recalibration-bump sensitivities
  validate calibration rather than merely exercising optimizer plumbing.
- New APIs are implemented in dal-public; bindings remain thin adapters.
- Published docs describe implemented behavior, with concise code comments and
  documentation-to-source links only.

## Design sources

Hyer, Derivatives Algorithms Volume 1, second edition: sections 13.1–13.2
(printed pages 267–273), 13.7 (290–294), 7.4 (111–116), and 14.7 (302–303).
QuantLib Gsr/G2 provide independent Gaussian comparisons. Schlenkrich's QuantLib
extension at 7353bf8e84f981934163fa7fb07fae2e68b5d4d3 supplies quasi-Gaussian and
annuity-weighted leverage-calibration references. Implement DAL-owned formulas
and tests rather than introducing a runtime dependency on those libraries.

## Review decisions

Proceed with caveats: preserve the legacy archive with distinct multi-factor data
types; condition discounting on all relevant states before Hybrid composition;
normalize variance and fix factor shape in the first calibration stage; use PSD
factorization with explicit rank handling; and validate calibration on independent
paths. FX requires a separate foreign-measure/quanto design and is outside the
initial rates delivery.
