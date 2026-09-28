"""One-factor GSR example and independent swap/swaption price oracles."""

from datetime import datetime
import math

TODAY = datetime(2026, 9, 28)
EXPIRY = datetime(2027, 9, 28)
FIRST_COUPON = datetime(2028, 3, 28)
MATURITY = datetime(2028, 9, 28)
CURVE_DATES = (TODAY, EXPIRY, FIRST_COUPON, MATURITY)
INPUT_RATE = 0.025
STRIKE = 0.03
VOLATILITY = 0.02

CONVENTIONS = {
    "valuation_date": TODAY.isoformat(),
    "currency": "USD",
    "input_curve": "2.5% continuously compounded ACT/365F; log-linear discount factors",
    "forward_start": EXPIRY.isoformat(),
    "maturity": MATURITY.isoformat(),
    "fixed_leg": "6M 30/360; pay fixed at 3%",
    "floating_leg": "3M ACT/360 IBOR; two-business-day fixing lag",
    "calendar": "weekends only; coupon dates need no business-day adjustment",
    "notional": 1.0,
    "volatility": "g=0.02 and H=1.0; QuantLib Gsr reversion=0 and volatility=0.02",
    "swaption_payoff": "max(value of underlying payer swap at expiry, 0)",
    "timing_boundary": "curves, swaps, model and script prepared before repeated pricing",
    "pricing_methods": {
        "swap": "DAL, QuantLib and rateslib deterministic single-curve IRS pricing",
        "swaption": "DAL Sobol script MC versus QuantLib Gsr Sobol path MC; rateslib unsupported",
    },
}


def cases(smoke=False):
    return [
        {
            "name": "gsr_swap_static_pv_32",
            "operation": "gsr_static_swap",
            "size": 4 if smoke else 32,
        },
        {
            "name": "gsr_swaption_price_65536",
            "operation": "gsr_swaption",
            "size": 4096 if smoke else 65536,
        },
    ]


def discount(date):
    return math.exp(-INPUT_RATE * (date - TODAY).days / 365.0)


def swap_pv():
    return (
        discount(EXPIRY)
        - discount(MATURITY)
        - STRIKE * 0.5 * (discount(FIRST_COUPON) + discount(MATURITY))
    )


def swaption_pv():
    # Under the expiry-forward measure, the centered GSR factor is N(0, g²T).
    variance = VOLATILITY**2 * (EXPIRY - TODAY).days / 365.0
    stdev = math.sqrt(variance)

    def bond(date, state):
        loading = (date - EXPIRY).days / 365.0
        return (
            discount(date)
            / discount(EXPIRY)
            * math.exp(-loading * state - 0.5 * loading**2 * variance)
        )

    def exercise_value(state):
        return (
            1.0
            - STRIKE * 0.5 * bond(FIRST_COUPON, state)
            - (1.0 + STRIKE * 0.5) * bond(MATURITY, state)
        )

    lower, upper = -1.0, 1.0
    if exercise_value(lower) >= 0.0 or exercise_value(upper) <= 0.0:
        raise ValueError("GSR swaption exercise boundary is not bracketed")
    for _ in range(80):
        midpoint = 0.5 * (lower + upper)
        if exercise_value(midpoint) < 0.0:
            lower = midpoint
        else:
            upper = midpoint
    boundary = 0.5 * (lower + upper)

    def normal_cdf(value):
        return 0.5 * math.erfc(-value / math.sqrt(2.0))

    price = discount(EXPIRY) * normal_cdf(-boundary / stdev)
    for date, coupon in (
        (FIRST_COUPON, STRIKE * 0.5),
        (MATURITY, 1.0 + STRIKE * 0.5),
    ):
        loading = (date - EXPIRY).days / 365.0
        price -= (
            coupon
            * discount(date)
            * normal_cdf((-boundary - loading * variance) / stdev)
        )
    return price


def expected(case):
    if case["operation"] == "gsr_static_swap":
        return [swap_pv()] * case["size"]
    if case["operation"] == "gsr_swaption":
        return [swaption_pv()]
    raise ValueError(f"unknown GSR operation: {case['operation']}")


def method(backend, case):
    if case["operation"] == "gsr_static_swap":
        return "single-curve IRS pricing"
    if backend == "dal":
        return "Sobol script Monte Carlo"
    return "Gsr process Sobol Monte Carlo, Python path payoff"


def tolerance(case, backend=None):
    if case["operation"] == "gsr_static_swap":
        return 1e-10
    return 2e-4
