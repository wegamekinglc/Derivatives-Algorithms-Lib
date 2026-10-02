import math

import dal
import pytest


def rates_model(g_values):
    today = dal.Date_(2026, 10, 2)
    dal.EvaluationDate_Set(today)
    curve = dal.GSRCurveData_New(
        "curve", today, "USD",
        [today, dal.Date_(2027, 10, 2), dal.Date_(2028, 10, 1)],
        [0.0, -0.03, -0.06], [], dal.DoubleMatrix_(0, 0),
    )
    vol = dal.MultiFactorGSRVolData_New(
        "vol", ["level", "slope"], [today], dal.DoubleMatrix_(g_values),
        [today], dal.DoubleMatrix_([[1.0], [-0.4]]),
        dal.DoubleMatrix_([[1.0, 0.3], [0.3, 1.0]]),
    )
    return dal.MultiFactorGSRModelData_New("rates", curve, vol)


@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("aad", [False, True])
def test_multi_factor_zero_vol_reprices_curve_and_node_risk(compiled, aad):
    model = rates_model([[0.0], [0.0]])
    product = dal.Product_New(
        [dal.Date_(2027, 10, 2)], ["pay PAYS FIX(IR[USD,DF,2028-10-01])"]
    )
    settings = dal.MonteCarloSettings_(compiled=compiled, enable_aad=aad)
    result = dal.MonteCarlo_ValueWithSettings(product, model, 16, simulation=settings)
    assert result["PV"] == pytest.approx(math.exp(-0.06), abs=1e-12)
    if aad:
        assert result["d_logdf:OIS:2028-10-01"] == pytest.approx(math.exp(-0.06), abs=1e-12)
        assert "d_g:level:2026-10-02" in result
        assert "d_H:slope:2026-10-02" in result


def test_multi_factor_stochastic_bond_with_bridge_reprices_curve():
    model = rates_model([[0.02], [0.01]])
    product = dal.Product_New(
        [dal.Date_(2027, 10, 2)], ["pay PAYS FIX(IR[USD,DF,2028-10-01])"]
    )
    settings = dal.MonteCarloSettings_(compiled=True, use_bb=True)
    result = dal.MonteCarlo_ValueWithSettings(product, model, 16384, simulation=settings)
    assert result["PV"] == pytest.approx(math.exp(-0.06), abs=5e-5)


def test_multi_factor_vol_rejects_indefinite_correlation():
    today = dal.Date_(2026, 10, 2)
    with pytest.raises(RuntimeError, match="positive semidefinite"):
        dal.MultiFactorGSRVolData_New(
            "vol", ["level", "slope"], [today], dal.DoubleMatrix_([[0.02], [0.01]]),
            [today], dal.DoubleMatrix_([[1.0], [0.4]]),
            dal.DoubleMatrix_([[1.0, 1.01], [1.01, 1.0]]),
        )
