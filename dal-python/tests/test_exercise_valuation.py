"""Python-layer valuation of an early-exercise product (LSMC with fuzzy AAD)."""

import math

import dal
import pytest

EVAL = dal.Date_(2026, 9, 20)
MID = dal.Date_(2027, 9, 20)
MATURITY = dal.Date_(2028, 3, 20)
SPOT, VOL, RATE, DIV, STRIKE = 100.0, 0.20, 0.05, 0.0, 100.0
EXERCISE = f"EXERCISE MAX({STRIKE} - spot(), 0.0)"


def _model(spot=SPOT):
    return dal.BSModelData_New(spot, VOL, RATE, DIV)


def _product(*dates):
    return dal.Product_New(list(dates), [EXERCISE] * len(dates))


def _value(product, model, paths, **simulation):
    return dal.MonteCarlo_ValueWithSettings(
        product,
        model,
        paths,
        valuation=dal.ScriptValuationSettings_(evaluation_date=EVAL),
        simulation=dal.MonteCarloSettings_(**simulation),
    )


def _black_put():
    time = (MATURITY - EVAL) / 365.0
    forward = SPOT * math.exp((RATE - DIV) * time)
    sd = VOL * math.sqrt(time)
    d1 = (math.log(forward / STRIKE) + 0.5 * sd * sd) / sd

    def cdf(x):
        return 0.5 * math.erfc(-x / math.sqrt(2.0))

    return math.exp(-RATE * time) * (STRIKE * cdf(-(d1 - sd)) - forward * cdf(-d1))


def test_bermudan_exercise_adds_value_over_the_european_leg():
    paths = 2**14
    european = _value(_product(MATURITY), _model(), paths)["PV"]
    bermudan = _value(_product(MID, MATURITY), _model(), paths)["PV"]
    black = _black_put()
    # exercise at maturity only is the European put: sobol agrees with the Black oracle
    assert abs(european - black) < 0.01
    # same paths and settings: the extra exercise date cannot subtract value
    assert bermudan > european
    # loose Python-layer band around the PDE-pinned early-exercise premium (~0.39 here)
    assert black + 0.1 < bermudan < black + 0.75


@pytest.mark.parametrize("compiled", [False, True])
def test_fuzzy_aad_spot_delta_matches_central_difference(compiled):
    paths, bump = 2**18, 0.05
    product = _product(MID, MATURITY)
    aad = _value(product, _model(), paths, enable_aad=True, compiled=compiled)
    assert set(aad) == {"PV", "d_spot", "d_vol", "d_rate", "d_div"}
    hard = _value(product, _model(), paths, compiled=compiled)["PV"]
    # fuzzy blending tracks the hard PV to the decision remnant (default smooth 0.01)
    assert abs(aad["PV"] - hard) < 1e-3
    up = _value(product, _model(SPOT + bump), paths, compiled=compiled)["PV"]
    down = _value(product, _model(SPOT - bump), paths, compiled=compiled)["PV"]
    finite_difference = (up - down) / (2 * bump)
    # the adjoint is the exact gradient of the frozen-policy functional; the residual
    # gap is the policy-regeneration envelope (measured ~0.3% at 2^18 sobol paths,
    # well inside the 5% band of the C++ suite)
    assert abs(aad["d_spot"] - finite_difference) < 0.05 * abs(finite_difference)
