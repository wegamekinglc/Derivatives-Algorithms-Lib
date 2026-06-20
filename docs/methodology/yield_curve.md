# Yield Curve Construction

This note describes the mathematics of the yield-curve framework: how discount
factors are represented, how a curve is parameterised, and how it is calibrated
to market instruments. The emphasis is on the financial mathematics and the
construction algorithm, not on the code that implements them.

## Mathematical Objects

A single-currency interest-rate curve is captured by the **discount factor**
$P(t_0, T)$ — today's value of one unit of currency paid at $T$. Everything else
is derived from it.

The library parameterises the curve by the **instantaneous forward rate $f(t)$**.
Time is measured in days, and the day-count integral is annualised by dividing by
$365$, so the discount factor is

$$
P(t_0, T) = \exp\!\left( -\frac{1}{365}\int_{t_0}^{T} f(t)\,dt \right).
$$

All other objects follow from this single definition.

- **Instantaneous forward rate** $f(t)$ is the continuously-compounded rate for an
  infinitesimal period at $t$; it is the quantity integrated above.

- **Zero (spot) rate** $z(T)$ is the constant rate giving the same discount factor.
  With $\tau = (T-t_0)/365$ the year fraction, $P(t_0,T) = e^{-z(T)\,\tau}$, so
  $z(T) = \tfrac{1}{T-t_0}\int_{t_0}^{T} f\,dt$.

- **Forward discount factor** between two future dates follows from the ratio:

  $$
  P(t_1, t_2) = \frac{P(t_0, t_2)}{P(t_0, t_1)} = \exp\!\left( -\frac{1}{365}\int_{t_1}^{t_2} f(t)\,dt \right).
  $$

Choosing $f$ as the state variable makes the integral — and therefore every
discount factor — a closed-form function of the curve parameters, which is what
makes both calibration and AAD sensitivities efficient.

## Forward-Rate Parameterisations

The forward curve $f(t)$ is described by a small number of parameters anchored at
**knot dates** $t_1 < t_2 < \dots < t_K$. Two parameterisations are used; both
precompute the cumulative integral $S(t) = \int_{t_0}^{t} f(u)\,du$ at each knot so
that a discount factor is a single subtraction and scaling, $P(t_1,t_2) =
\exp\!\big(-(S(t_2) - S(t_1))/365\big)$.

### Piecewise-Constant Forwards

$f(t)$ is a step function: it holds the value $f_i$ on $[t_i, t_{i+1})$. This uses
$K$ parameters (one per knot). The cumulative integral is exact and trivial:

$$
S(t_i) = S(t_{i-1}) + (t_i - t_{i-1})\,f_{i-1},
$$

and for a date $t \in [t_{i-1}, t_i)$,

$$
S(t) = S(t_{i-1}) + (t - t_{i-1})\,f_{i-1}.
$$

Before the first knot the rate is held flat at $f_1$.

### Piecewise-Linear Forwards

$f(t)$ is continuous and linear between knots, with separate left- and
right-limit values at each knot to allow controlled kinks. This uses $2K$
parameters. Within $[t_i, t_{i+1}]$ the rate interpolates linearly,

$$
f(t) = f_i^{R} + \frac{t - t_i}{t_{i+1}-t_i}\left(f_{i+1}^{L} - f_i^{R}\right),
$$

and the integral over a knot interval is the trapezoid area,

$$
S(t_i) = S(t_{i-1}) + \tfrac{1}{2}\left(f_{i-1}^{R} + f_i^{L}\right)(t_i - t_{i-1}).
$$

Outside the knot range $f$ is held flat at the nearest endpoint value.

Piecewise-linear forwards give a smoother, more realistic curve (continuous
forwards) at the price of twice as many parameters; piecewise-constant forwards
are simpler and more robust. The calibration layer can use either.

## Deriving Market Rates from the Curve

Once $f(t)$ — hence $P(\cdot,\cdot)$ — is known, observable rates follow from
no-arbitrage relations.

**Forward LIBOR / IBOR.** A simply-compounded forward rate for the accrual period
$[T, T+\tau]$ is

$$
L(T, T+\tau) = \frac{1}{\tau}\left( \frac{1}{P(T, T+\tau)} - 1 \right),
$$

where $\tau$ is the year fraction under the relevant day-count convention. This is
the standard relation that a forward rate agreement has zero value at inception.

**Swap par rate.** The fixed rate that makes a vanilla swap worth zero is the
ratio of the floating-leg PV to the fixed-leg annuity, both computed from the same
discount factors.

These relations turn the curve into a pricing engine for the calibration
instruments (deposits, futures/STIR, swaps).

## Multi-Curve Framework

Post-2008 markets discount and forecast on *different* curves. The framework
supports this through **base-curve layering**: a curve's discount factor is the
product of its own factor and that of an optional base curve,

$$
P(t_1,t_2) = P_{\text{spread}}(t_1,t_2)\,\cdot\,P_{\text{base}}(t_1,t_2).
$$

The typical construction uses an OIS curve as the base for discounting, with
tenor-specific spread curves layered on top to forecast each IBOR tenor (1M, 3M,
6M, ...). Discounting requests are routed to the appropriate collateral curve,
falling back to OIS when a specific collateral curve is absent; forecasting is
routed to the relevant tenor curve, falling back to the single discount curve in a
single-curve setup.

Because the spread is multiplicative in discount factors (additive in the
integrated forward rate), the dependency of derived curves on their base is exact
and is tracked so that a bump to the base curve flows consistently through every
curve that layers on it — the basis for bump-and-reprice risk.

## Calibration as a Root-Finding Problem

Calibration finds the forward-rate parameters $x$ (the $f_i$, or $f_i^L,f_i^R$)
that reprice a set of market instruments. For each instrument $j$ define a
**residual**

$$
r_j(x) = \text{model rate}_j(x) - \text{market rate}_j,
$$

and seek $x$ with $r_j(x) = 0$ for all $j$. Each residual is a smooth function of
$x$ through the discount-factor integrals, so the system can be solved by
Newton-type iteration, and the Jacobian $\partial r_j/\partial x_i$ is available
analytically via AAD.

### Underdetermination and Smoothness

A piecewise-linear curve carries $2K$ parameters but the market often quotes fewer
than $2K$ instruments, so the system is **underdetermined**: infinitely many
curves reprice the data. A unique, well-behaved solution is selected by preferring
the *smoothest* curve — the one that minimises a weighted norm of parameter
movements:

$$
\min_x \; \tfrac{1}{2}\,(x - x_0)^{\mathsf T} W\, (x - x_0)
\quad \text{subject to} \quad r(x) = 0,
$$

with $W$ a symmetric positive-definite weight matrix. Choosing $W$ to penalise
differences between neighbouring knots (a tridiagonal smoothing operator)
suppresses spurious oscillation between instrument maturities. The mechanics of
this constrained minimisation — and the approximate-fit variant for inconsistent
quotes — are covered in [Underdetermined search](underdetermined_search.md).

### Bootstrapping Order

Instruments are ordered by maturity so that the curve is built outwards in time:
short-dated instruments pin the near end of the curve, longer-dated instruments
extend it. Knots can be supplied by the caller, taken from the instrument
maturities, or formed from the union of both. After the solve, repricing residuals
(RMS and maximum absolute error) quantify the fit quality, and the effective
(weighted) Jacobian inverse is retained so that risk sensitivities can be obtained
without re-solving the system. That inverse carries a `tolerance_` factor from the
solver's residual scaling — see [Yield-Curve Jacobian and Inverse-Jacobian
Risk](yield_curve_jacobian.md) before consuming it.

## Construction Pipeline

```text
market instruments (deposits, STIR, swaps)
        │  define residuals r_j(x) = model_j(x) − market_j
        ▼
underdetermined solver  (min ½‖x−x₀‖²_W  s.t. r(x)=0)
        │  Jacobian via AAD; smoothness via weight matrix W
        ▼
forward-rate parameters x   (piecewise constant: K, piecewise linear: 2K)
        │  cumulative integrals S(t) precomputed at knots
        ▼
discount factors  P(t₁,t₂) = exp(−∫f dt / 365) × P_base
        │
        ▼
multi-curve routing: OIS discounting + tenor forecasting
```

## Joint Simultaneous Calibration and AAD Analytic Jacobian

`CalibrateJointMultiCurve` (`dal-cpp/dal/curve/jointcalibration.hpp`) solves
all discount and forward curves simultaneously from one **stacked parameter
vector** $x$. Each curve is declared by a `JointCurveDeclaration_` and the
joint spec carries a single `solveMode_`, `tolerance_`, and `smoothingWeight_`.
The smoother is **block-diagonal**: each declaration's own $2K$ parameters are
penalised independently, so a knot perturbation on the OIS curve does not
directly enter the 3M-curve's smoothing norm.

### AAD Analytic Jacobian

The joint calibration residual function overrides
`Underdetermined::Function_::Gradient` (`dal-cpp/dal/curve/jointcalibration.cpp`)
with a backend-neutral reverse-sweep Jacobian. On an eligible spec the solver
receives an exact analytic Jacobian $J_{ij} = \partial r_i / \partial x_j$
rather than running $P+1$ dense-bump evaluations.

**Recording contract.** The contract that works identically on all four AAD
backends (native, XAD, CoDiPack, Adept) is:

$$
\text{Clear} \rightarrow \text{RegisterIndependent}(x_k) \;\forall k \;
\rightarrow \text{NewRecording} \rightarrow \text{forward pass} \rightarrow
\text{per-row } \{\text{ZeroAdjoints},\; \bar{r}_i = 1,\;
\text{PropagateToStart},\; \text{harvest}\}.
$$

Under piecewise-linear forwards each declaration contributes
$2 \cdot n_{\text{knots}}$ independents -- every knot (including knot 0) is free;
there is no anchor exclusion (unlike the single-curve LOG_DISCOUNT path).

**Tape-layer primitives.** Three new templated types extend the `Dal::Tape`
namespace for curve calibration:

| Type                                  | Role                                                                      | Header                                             |
|---------------------------------------|---------------------------------------------------------------------------|----------------------------------------------------|
| `Tape::DiscountPWLF_<T_, B_>`         | PWL-forward curve on `T_` with optional templated base handle             | `dal-cpp/dal/curve/ycpwlf.hpp`                     |
| `Tape::JointCurveBlock_<T_>`          | Multi-curve routing context: `Discount(collateral)` and `Forward(tenor,collateral)` reads in the `T_` domain | `dal-cpp/dal/curve/jointycctx.hpp` |
| `Tape::JointRate_<T_>`                | Projection-capable rate base: `operator()(const JointCurveBlock_<T_>&)`   | `dal-cpp/dal/curve/jointrate.hpp`                  |

The double specialisation of `Tape::DiscountPWLF_` is byte-for-byte identical
to the existing anonymous-namespace `DiscountPWLF_` at
`dal-cpp/dal/curve/ycimp.cpp`. The `Number_` specialisation is constructed only
by the AAD `Gradient` override, with a two-pass build: discount declarations
first as baseless `Tape::DiscountPWLF_<Number_>`, then forward declarations as
base-layered `Tape::DiscountPWLF_<Number_, DiscountCurve_<Number_>>` with the
discount declaration's curve as a `Number_`-typed base handle -- so the reverse
sweep propagates OIS adjoints through the forward curve's base multiplication
into the OIS discount-curve free nodes.

**OIS-discount vs IBOR-projection routing.** The OIS-discount slice (where
$\text{forecast} = \text{discount}$) rides the inherited
`Swap_::PrecomputeT<T_>` -- both the AAD path and the double path share the
identical simple-rate arithmetic, so the Jacobian is correct. The IBOR
projection slice (where $\text{forecast}(3M) \neq \text{discount}(OIS)$) is
priced through the new `Tape::JointRate_<T_>` hierarchy:
`DepositRateProj_<T_>`, `ForwardRateProj_<T_>` (covering FRA and Future), and
`SwapRateProj_<T_>`, each reading both a discount and a forecast curve via the
`JointCurveBlock_<T_>` routing context.

**Eligibility.** The AAD path engages only when every joint declaration
satisfies:

- `parameterization_ == PIECEWISE_LINEAR_FWD`,
- every instrument is a `Deposit_`, `FRA_`, `Future_`, or `Swap_` (including
  `OISSwap_`, which inherits `Swap_` and rides `Swap_::PrecomputeT<T_>`),
- `liborBasis_ == ACT_365F` (agrees with the `DAYS_PER_YEAR = 365.0`
  denominator the templated curve assumes),
- base-layered forward declarations resolve their base collateral against a
  PIECEWISE_LINEAR_FWD discount declaration in the same spec.

Each failing condition emits a one-time `NOTICE` naming the declaration index
and the condition; the solver then dense-bumps unchanged. The verdict is
evaluated once per `CalibrateJointMultiCurve` call and cached, so every
`NOTICE` fires at most once.

**Options and result.** `JointMultiCurveCalibrationOptions_` carries the
`CurveJacobianMode_ jacobianMode_` field, defaulting to `ANALYTIC` (matching
the single-curve default). The single-arg `CalibrateJointMultiCurve(spec)`
delegates to the two-arg overload with a default-constructed options, so
existing callers exercise the AAD path on eligible specs.

The `JointMultiCurveCalibrationResult_` struct provides
`jacobianAtSolution_` -- the unscaled analytic forward Jacobian
$d(\text{residual}_i) / d(\text{param}_j)$ at the solved point, shape
`(totalResiduals) x (totalFreeParams)`. Populated only when
`jacobianMode_ == ANALYTIC` AND the spec is eligible AND
`solveMode_ == EXACT`; empty otherwise. The Jacobian is stored in a shared
`XCurveJacobian_` (`dal-cpp/dal/curve/curvejacobian.hpp`) -- the same dense
subclass used by the single-curve AAD path.

**Backend coverage.** The analytic path compiles and produces a correct
Jacobian under all four AAD backends (native, Adept, XAD, CoDiPack), verified
by element-wise agreement against a central finite-difference bump of the
joint residual function. The oracle tests live at
`dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp`.

## Examples

The snippets below are condensed from the standalone example programs under
`dal-cpp/examples/` and adapt their real call sequences. They show the public
calibration surface declared in `dal-cpp/dal/curve/calibration.hpp`,
`dal-cpp/dal/curve/curveblock.hpp`, and `dal-cpp/dal/curve/ycinstrument.hpp`.
Class and enum names, factory functions, and include paths match the current
source; see the citations in each example.

### Single-curve bootstrap from deposits, futures, and swaps

This mirrors `dal-cpp/examples/euribor3m_curve/euribor3m_curve.cpp`: a classic
single-curve Euribor 3M bootstrap that discounts and forecasts off one curve
(no OIS data). The instrument set is deposit + STIR futures + vanilla swaps,
assembled into a `CurveCalibrationSpec_` and solved by `CalibrateYieldCurve`.

```cpp
#include <dal/curve/calibration.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

// Trade date and spot, on the TARGET calendar (Euribor T+2).
const Date_ today(2026, 4, 30);
const Holidays_ target("TARGET");

// Single-curve conventions: forecast routes to the discount curve.
RateIndexConvention_ euribor3m = Ccy::Conventions::LiborIndex()(Ccy_("EUR"));
euribor3m.useProjectionCurve_ = false;            // single curve: 3M forecast == discount
euribor3m.forecastTenor_       = PeriodLength_("3M");
euribor3m.dayBasis_            = DayBasis_("ACT_360");
euribor3m.fixingHolidays_      = target;
euribor3m.accrualHolidays_     = target;

RateLegConvention_ fixedLeg = Ccy::Conventions::SwapFixedLeg()(Ccy_("EUR"));
fixedLeg.paymentFrequency_ = PeriodLength_("12M");
fixedLeg.dayBasis_         = DayBasis_("30_360");
fixedLeg.accrualHolidays_  = target;
fixedLeg.paymentHolidays_  = target;

RateLegConvention_ floatLeg = Ccy::Conventions::SwapFloatLeg()(Ccy_("EUR"));
floatLeg.paymentFrequency_ = PeriodLength_("3M");
floatLeg.dayBasis_         = DayBasis_("ACT_360");

// Instruments: a 3M cash deposit, a serial 3M future, and a vanilla IRS.
const Handle_<YCInstrument_> deposit(new Deposit_(today,
                                                  spot,
                                                  Date::AddMonths(spot, 3),
                                                  0.021990,            // 3M cash rate
                                                  euribor3m));
const Handle_<YCInstrument_> future(new Future_(today,
                                                Date_(2026, 6, 17),  // IMM settle (3rd Wed)
                                                Date_(2026, 9, 16),  // underlying 3M end
                                                0.0224992,           // convexity-adjusted rate
                                                euribor3m,
                                                0.0));
const Handle_<YCInstrument_> swap(new Swap_(today,
                                            spot,
                                            Date::AddMonths(spot, 60), // 5Y
                                            0.0279255,                  // mid swap rate
                                            fixedLeg,
                                            euribor3m,
                                            floatLeg));

// Assemble the spec: one pillar per instrument maturity, piecewise-linear forwards,
// EXACT solve. (For an overdetermined system — more quotes than free knots — switch
// solveMode_ to CurveSolveMode_::Value_::APPROXIMATE; see the multi-curve example.)
CurveCalibrationSpec_ spec;
spec.today_                 = today;
spec.ccy_                   = "EUR";
spec.curveName_             = "euribor3m";
spec.targetCollateral_      = CollateralType_(CollateralType_::Value_::OIS);
spec.calibrateDiscountCurve_ = true;
spec.parameterization_      = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
spec.knotPolicy_            = CurveKnotPolicy_::Value_::INPUT;
spec.liborBasis_            = DayBasis_("ACT_365F");
spec.instruments_           = {deposit, future, swap};
spec.knotDates_             = {deposit->TimeSpan().second,
                               future->TimeSpan().second,
                               swap->TimeSpan().second};

const CurveCalibrationResult_ result = CalibrateYieldCurve(spec);

// Read discount factors and continuously-compounded zero rates off the calibrated curve.
for (const int months : {6, 12, 24, 60, 120}) {
    const Date_ d   = Date::AddMonths(today, months);
    const double df = (*result.curve_)(today, d);
    const double yf = static_cast<double>(d - today) / 365.0;
    const double z  = -std::log(df) / yf;       // continuously-compounded zero
    // ...
}
```

The `CurveCalibrationDiagnostics_` returned in `result.diagnostics_` carries
per-instrument market/model rates and residuals, plus `rmsResidual_` and
`maxAbsResidual_`; the `effJacobianInverse_` matrix maps quote bumps to
forward-rate parameters without re-solving. Note that its units include an extra
`tolerance_` factor (the solver scales residuals by `1/tolerance_` before forming
the pseudoinverse), so a sensitivity transform must read
`r = gᵀ · effJacobianInverse_ / tolerance_` — see [Yield-Curve Jacobian and
Inverse-Jacobian Risk](yield_curve_jacobian.md).

API citations:

- `CurveCalibrationSpec_` fields and `CalibrateYieldCurve` overloads — `dal-cpp/dal/curve/calibration.hpp`.
- `Deposit_`, `Future_`, `Swap_` constructors — `dal-cpp/dal/curve/ycinstrument.hpp`.
- `DiscountCurve_::operator()(Date_, Date_)` returns the discount factor — `dal-cpp/dal/curve/discount.hpp`.

### Sequential multi-curve calibration (OIS discounting + tenor forecasting)

This mirrors `dal-cpp/examples/curve_calibration/curve_calibration.cpp`. Two
stages share a knot grid: stage 1 calibrates the OIS discount curve from OIS
deposits and OIS swaps; stage 2 calibrates the 3M forecasting curve from FRAs
and IRS, holding the discount curve fixed. Each stage quotes 20 instruments
onto 9 knots, an overdetermined system, so both stages use the least-squares
`APPROXIMATE` solver.

```cpp
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/protocol/collateraltype.hpp>

// Stage 1 — OIS discount curve.
CurveCalibrationSpec_ oisStage;
oisStage.today_                = today;
oisStage.ccy_                  = "USD";
oisStage.curveName_            = "ois";
oisStage.targetCollateral_     = CollateralType_(CollateralType_::Value_::OIS);
oisStage.solveMode_            = CurveSolveMode_::Value_::APPROXIMATE;
oisStage.fitTolerance_         = 1e-8;
oisStage.knotDates_            = {Date::AddMonths(today, 1),  Date::AddMonths(today, 3),
                                  Date::AddMonths(today, 6),  Date::AddMonths(today, 12),
                                  Date::AddMonths(today, 24), Date::AddMonths(today, 36),
                                  Date::AddMonths(today, 60), Date::AddMonths(today, 84),
                                  Date::AddMonths(today, 120)};
oisStage.instruments_          = /* OIS deposits + OISSwap_ handles */;

// Stage 2 — 3M forecasting curve, discount curve held fixed.
CurveCalibrationSpec_ liborStage;
liborStage.today_                 = today;
liborStage.ccy_                   = "USD";
liborStage.curveName_             = "libor3m";
liborStage.calibrateDiscountCurve_ = false;          // forecast-only stage
liborStage.targetCollateral_      = CollateralType_(CollateralType_::Value_::OIS);
liborStage.targetTenor_           = libor3m.forecastTenor_;  // PeriodLength_("3M")
liborStage.knotDates_             = oisStage.knotDates_;
liborStage.solveMode_             = CurveSolveMode_::Value_::APPROXIMATE;
liborStage.fitTolerance_          = 1e-8;
liborStage.instruments_           = /* FRA_ + Swap_ handles */;

MultiCurveCalibrationSpec_ multi;
multi.name_       = "usd_example";
multi.ccy_        = "USD";
multi.liborBasis_ = libor3m.dayBasis_;
multi.stages_     = {oisStage, liborStage};

const MultiCurveCalibrationResult_ result = CalibrateMultiCurve(multi);

// Reprice a 3x6 FRA off the calibrated multi-curve block.
const CurveBlock_ bundle("usd_example",
                         "USD",
                         result.discountCurves_,
                         result.forwardCurves_,
                         libor3m.dayBasis_);
const auto fraRate = fra3x6->Precompute(Handle_<YieldCurve_>());
const double modelFra = (*fraRate)(bundle);
```

API citations:

- `OISSwap_` and `FRA_` constructors — `dal-cpp/dal/curve/ycinstrument.hpp`.
- `MultiCurveCalibrationSpec_` and `CalibrateMultiCurve` — `dal-cpp/dal/curve/calibration.hpp`.
- `CurveBlock_` multi-curve constructor — `dal-cpp/dal/curve/curveblock.hpp`.

### Flat starting curve for synthetic examples

When no market quotes are available (e.g. building a market to derive implied
quotes), a flat forward curve is built directly with `NewDiscountPWLF`. This is
the helper used in both `curve_calibration.cpp` and
`xccy_curve_calibration.cpp`:

```cpp
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>

const Vector_<Date_> knots = {Date::AddMonths(today, 1),  Date::AddMonths(today, 3),
                              Date::AddMonths(today, 6),  Date::AddMonths(today, 12),
                              Date::AddMonths(today, 24), Date::AddMonths(today, 36),
                              Date::AddMonths(today, 60), Date::AddMonths(today, 84),
                              Date::AddMonths(today, 120)};
const Vector_<> values(knots.size(), 0.02);   // flat 2% forward rate

// Layer an optional base curve by passing it as the last argument (multi-curve
// spread construction — see the multi-curve framework above).
Handle_<DiscountCurve_> flat(
    NewDiscountPWLF("flat", "USD", PiecewiseLinear_(knots, values, values)));
```

API citations:

- `PiecewiseLinear_(knots, fLeft, fRight)` — `dal-cpp/dal/curve/piecewiselinear.hpp`.
- `NewDiscountPWLF(name, ccy, fwds, base)` — `dal-cpp/dal/curve/ycimp.hpp`.

## See Also

- [Underdetermined search](underdetermined_search.md) — the optimisation method
  that performs the calibration solve.
- [Cross-currency calibration](xccy_calibration.md) — extends this framework to a
  cross-currency basis curve.
- [AAD methodology](aad.md) — supplies the analytic Jacobian used in calibration
  and the curve risk sensitivities.
