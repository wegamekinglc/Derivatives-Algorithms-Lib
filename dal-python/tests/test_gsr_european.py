import dal
import pytest


def model(g=0.02):
    today = dal.Date_(2026, 10, 2)
    curve = dal.GSRCurveData_New(
        "curve", today, "USD", [today, dal.Date_(2029, 10, 1)],
        [0.0, -0.09], [], dal.DoubleMatrix_(0, 0),
    )
    vol = dal.MultiFactorGSRVolData_New(
        "vol", ["level"], [today], dal.DoubleMatrix_([[g]]),
        [today], dal.DoubleMatrix_([[1.0]]), dal.DoubleMatrix_([[1.0]]),
    )
    return dal.MultiFactorGSRModelData_New("rates", curve, vol)


def bond_option():
    return dal.BondOption_(dal.Date_(2027, 10, 2), dal.Date_(2028, 10, 1), 0.97, "CALL")


def test_bond_price_and_calibration_recover_volatility():
    option = bond_option()
    target = dal.GSR_EuropeanOptionPrice(model(), option)
    assert target.price > 0
    assert target.numerical_error == 0
    quote = dal.CalibrationQuote_("bond", option, target.price, 1e-6)
    parameter = dal.GSRCalibrationParameter_(0, 0, 0.0, 0.1)
    result = dal.Calibrate_GSRVolatility(model(0.009), [quote], [parameter])
    assert result.converged
    assert result.fit_within_tolerance
    assert result.numerical_validation_passed
    assert result.parameters[0] == pytest.approx(0.02, abs=1e-9)
    assert dal.GSR_EuropeanOptionPrice(result.model, option).price == pytest.approx(target.price, abs=1e-12)
    assert result.jacobian_rank == 1


def test_one_period_swaption_matches_caplet():
    expiry, end = dal.Date_(2027, 10, 2), dal.Date_(2028, 10, 1)
    swaption = dal.Swaption_(expiry, [dal.FixedCoupon_(end, 1.0)],
                              [dal.FloatingCoupon_(expiry, expiry, end, end, 1.0, 1.0, "12M")], 0.03, "CALL")
    caplet = dal.Caplet_(expiry, expiry, end, end, 1.0, 1.0, "12M", 0.03, "CALL")
    assert dal.GSR_EuropeanOptionPrice(model(), swaption).price == pytest.approx(
        dal.GSR_EuropeanOptionPrice(model(), caplet).price, abs=1e-12)


def test_public_errors_identify_invalid_inputs():
    with pytest.raises(RuntimeError, match="model is required"):
        dal.GSR_EuropeanOptionPrice(None, bond_option())
    quote = dal.CalibrationQuote_("bond", bond_option(), 0.01, 0.0)
    with pytest.raises(RuntimeError, match="price scales"):
        dal.Calibrate_GSRVolatility(model(), [quote], [dal.GSRCalibrationParameter_(0, 0, 0.0, 0.1)])
