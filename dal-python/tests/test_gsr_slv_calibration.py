import math

import dal
import pytest

from test_gsr_european import bond_option, model


def slv(level=1.0, eta=0.6, gaussian=None):
    settings = dal.GSRSLVSettings_()
    settings.vol_of_vol = eta
    settings.max_step = 0.25
    return dal.GSRSLVModelData_New("smile", gaussian or model(),
        dal.GSRLeverageData_New("leverage", [0.0], [0.0], dal.DoubleMatrix_([[level]])), settings)


def calibration_settings():
    settings = dal.GSRSLVCalibrationSettings_()
    settings.pricing.paths = 1024
    settings.validation.paths = 2048
    return settings


def test_price_fit_and_regularized_quote_risk():
    option = bond_option()
    settings = calibration_settings()
    target = dal.GSRSLV_EuropeanOptionPrices(slv(1.2), [option], settings.pricing)[0]
    assert target.price > 0
    assert target.standard_error > 0
    quotes = [dal.GSRCalibrationQuote_("bond", option, target.price, 0.005)]
    parameters = [dal.GSRSLVCalibrationParameter_("leverage:0:0", 0.2, 2.0)]
    fit = dal.Calibrate_GSRSLV(slv(0.8), quotes, parameters, settings)
    assert fit.converged
    assert fit.fit_within_tolerance
    assert fit.parameters == pytest.approx([1.2], abs=1e-5)
    assert fit.jacobian_rank == 1
    assert isinstance(fit.standard_errors, list)
    assert isinstance(fit.active_bounds, list)
    settings.solver.prior_weight = 2.0
    risk = dal.GSRSLV_QuoteRisk(slv(0.8), quotes, parameters, [option], settings)
    assert risk.calibration.converged
    assert risk.quote_names == ["bond"]
    assert risk.quote_units == ["PRICE_PER_NOTIONAL"]
    assert risk.stable == [True]
    assert not risk.curve_risk_included
    assert 0 < risk.sensitivities[0, 0] < 0.9
    assert dal.GSRSLV_EuropeanOptionPrices(fit.model, [option], settings.pricing)[0].price == pytest.approx(target.price, abs=1e-8)


def test_invalid_models_labels_and_validation_seeds():
    option = bond_option()
    with pytest.raises(RuntimeError, match="model is required"):
        dal.GSRSLV_EuropeanOptionPrices(None, [option])
    with pytest.raises(RuntimeError, match="GSRSLVModelData"):
        dal.GSRSLV_EuropeanOptionPrices(model(), [option])
    quote = dal.GSRCalibrationQuote_("bond", option, 0.01, 0.005)
    settings = calibration_settings()
    with pytest.raises(RuntimeError, match="unknown parameter"):
        dal.Calibrate_GSRSLV(slv(), [quote], [dal.GSRSLVCalibrationParameter_("missing", 0.1, 2.0)], settings)
    settings.validation.seed = settings.pricing.seed
    with pytest.raises(RuntimeError, match="distinct"):
        dal.Calibrate_GSRSLV(slv(), [quote], [dal.GSRSLVCalibrationParameter_("leverage:0:0", 0.1, 2.0)], settings)


def test_curve_quote_bridge_validates_snapshot_and_adds_curve_risk():
    from test_quote_risk import _single_quote_risk_inputs

    _, _, calibrated, _, market, _, provenance, _ = _single_quote_risk_inputs()
    today = dal.Date_(2025, 6, 20)
    nodes = [today, today.AddDays(365), today.AddDays(730), today.AddDays(1095)]
    curve = dal.GSRCurveData_New("snapshot", today, "USD", nodes,
        [math.log(calibrated.curve_(today, date)) for date in nodes], [], dal.DoubleMatrix_(0, 0))
    bridge = dal.GSRCurveQuoteRisk_New(curve, market, provenance, "discount")
    assert len(bridge.quote_names) == 3
    assert bridge.log_df_quote_jacobian.Rows() == 4
    vol = dal.MultiFactorGSRVolData_New("vol", ["level"], [today], dal.DoubleMatrix_([[0.02]]),
        [today], dal.DoubleMatrix_([[1.0]]), dal.DoubleMatrix_([[1.0]]))
    initial = slv(gaussian=dal.MultiFactorGSRModelData_New("rates", curve, vol))
    option = dal.GSRBondOption_(nodes[1], nodes[2], 0.96)
    settings = calibration_settings()
    settings.solver.prior_weight = 2.0
    price = dal.GSRSLV_EuropeanOptionPrices(initial, [option], settings.pricing)[0].price
    quotes = [dal.GSRCalibrationQuote_("bond", option, price, 0.005)]
    parameters = [dal.GSRSLVCalibrationParameter_("leverage:0:0", 0.2, 2.0)]
    risk = dal.GSRSLV_QuoteRisk(initial, quotes, parameters, [option], settings, curve_risk=bridge)
    assert risk.curve_risk_included
    assert len(risk.quote_names) == 4
    assert risk.quote_units[1:] == ["DECIMAL_QUOTE"] * 3
    assert any(abs(risk.sensitivities[0, col]) > 1e-4 for col in range(1, 4))
    with pytest.raises(RuntimeError, match="snapshot mismatch"):
        dal.GSRSLV_QuoteRisk(slv(), quotes, parameters, [option], settings, curve_risk=bridge)


def test_market_volatility_conversion_calibration_and_risk():
    today = dal.Date_(2026, 10, 2)
    exercise, end = today.AddDays(365), today.AddDays(730)
    curve = dal.GSRCurveData_New("market", today, "USD", [today, today.AddDays(1825)],
        [0.0, -0.15], [], dal.DoubleMatrix_(0, 0))
    vol = dal.MultiFactorGSRVolData_New("vol", ["level"], [today], dal.DoubleMatrix_([[0.02]]),
        [today], dal.DoubleMatrix_([[1.0]]), dal.DoubleMatrix_([[1.0]]))
    initial = slv(gaussian=dal.MultiFactorGSRModelData_New("rates", curve, vol))
    option = dal.GSRCaplet_(exercise, exercise, end, end, 1.0, 1.0, "12M", math.expm1(0.03))
    quote = dal.GSRMarketQuote_("normal", option, 0.02, 0.01)
    converted = dal.GSRMarketQuotes_Get_Prices(curve, [quote])[0]
    assert converted.price == pytest.approx(math.exp(-0.06) * 0.02 / math.sqrt(2 * math.pi), abs=1e-14)
    settings = calibration_settings()
    settings.use_aad_jacobian = True
    settings.solver.prior_weight = 1.0
    parameters = [dal.GSRSLVCalibrationParameter_("leverage:0:0", 0.2, 2.0)]
    fit = dal.Calibrate_GSRSLVMarket(initial, [quote], parameters, settings)
    assert fit.converged
    assert fit.conditional_errors == [0.0]
    risk = dal.GSRSLV_MarketQuoteRisk(initial, [quote], parameters, [option], settings)
    assert risk.quote_units == ["NORMAL_VOL"]
    assert risk.sensitivities[0, 0] > 0
    with pytest.raises(RuntimeError, match="bond options"):
        dal.GSRMarketQuotes_Get_Prices(curve, [dal.GSRMarketQuote_("bad", bond_option(), 0.02, 0.01)])


def test_conditional_lagged_swaption_diagnostics():
    today = dal.Date_(2026, 10, 2)
    exercise, fixing = today.AddDays(365), today.AddDays(540)
    start, end, pay = fixing.AddDays(2), fixing.AddDays(367), fixing.AddDays(369)
    option = dal.GSRSwaption_(exercise, [dal.GSRFixedCoupon_(pay, 1.0)],
        [dal.GSRFloatingCoupon_(fixing, start, end, pay, 1.0, 1.0, "12M")], 0.03)
    settings = dal.GSRMonteCarloSettings_()
    settings.paths = 256
    settings.conditional_paths = 16
    price = dal.GSRSLV_EuropeanOptionPrices(slv(), [option], settings)[0]
    assert price.price > 0
    assert price.standard_error > 0
    assert price.conditional_error >= 0
