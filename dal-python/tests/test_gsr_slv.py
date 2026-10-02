import math

import dal
import pytest

from test_gsr_european import model


def leverage():
    return dal.GSRLeverageData_New("leverage", [-0.02, 0.02], [0.0], dal.DoubleMatrix_([[1.0], [1.0]]))


@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("aad", [False, True])
def test_zero_vol_bond_with_variance_bridge_and_aad(compiled, aad):
    today = dal.Date_(2026, 10, 2)
    dal.EvaluationDate_Set(today)
    settings = dal.GSRSLVSettings_()
    settings.variance_correlations = [0.4]
    assert settings.variance_correlations == [0.4]
    settings.max_step = 0.25
    smile = dal.GSRSLVModelData_New("smile", model(0.0), leverage(), settings)
    product = dal.Product_New([dal.Date_(2027, 10, 2)], ["pay PAYS FIX(IR[USD,DF,2028-10-01])"])
    simulation = dal.MonteCarloSettings_(compiled=compiled, enable_aad=aad, use_bb=True)
    result = dal.MonteCarlo_ValueWithSettings(product, smile, 16, simulation=simulation)
    assert result["PV"] == pytest.approx(math.exp(-0.06), abs=1e-12)
    if aad:
        assert result["d_volOfVol"] == 0.0
        assert result["d_leverage:0:0"] == 0.0


def test_invalid_settings_and_wrong_model_are_rejected():
    settings = dal.GSRSLVSettings_()
    settings.variance_correlations = [1.01]
    with pytest.raises(RuntimeError, match="positive semidefinite"):
        dal.GSRSLVModelData_New("bad", model(), leverage(), settings)
    with pytest.raises(RuntimeError, match="MultiFactorGSRModelData"):
        dal.GSRSLVModelData_New("bad", dal.BSModelData_New(100.0, 0.2, 0.03, 0.0), leverage())
    with pytest.raises(RuntimeError, match="strictly positive"):
        dal.GSRLeverageData_New("bad", [0.0], [0.0], dal.DoubleMatrix_([[0.0]]))
