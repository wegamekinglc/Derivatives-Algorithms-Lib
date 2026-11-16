"""Frozen /2 observations, EXACT calibration and fixed-contract quote DV01."""

import gc

import dal
from float_format import format_float


def contract(mode):
    today, start, end, pay = [dal.Date_(2024, 3, day) for day in (1, 5, 8, 12)]
    counts = ({"lookback_business_days": 1} if mode in ("Lookback", "ObservationShift")
              else {"lockout_business_days": 1} if mode == "Lockout" else {})
    trade = dal.CompoundedOisScriptSpec_(instrument_id=f"python-{mode}", direction="PayFixed", notional=100.0, coupon=0.02,
        currency="USD", collateral_currency="USD", collateral_type="OIS", discount_route="USD.OIS", projection_route="USD.SOFR",
        fixing_index="USD-SOFR", observation_basis="ACT_360", fixed_basis="ACT_365F", rule_id="dal.compounded-ois-cashflows/2",
        observation_convention=dal.CompoundedOisObservationConvention_(mode, **counts),
        calendar=dal.HolidayCenterData_(center="explicit", holidays=[dal.Date_(2024, 3, 4)], work_weekends=[dal.Date_(2024, 3, 9)]),
        fixed_leg=[dal.CompoundedOisFixedRow_(start=start, end=end, payment=pay, accrual=3 / 365)],
        coupons=[dal.CompoundedOisCouponRow_(coupon_id="coupon", start=start, end=end, payment=pay)])
    valuation = dal.ScriptValuationSettings_(evaluation_date=today, today_fixing="Model", fixings=dal.MarketFixingSnapshot_New({}))
    prepared = dal.CompoundedOis_Prepare(trade, valuation)
    instrument = dal.CompoundedOisScriptInstrument_New(prepared)
    geometry = dal.CurveInstrumentGeometry_(today=today, currency="USD", knot_dates=[today, dal.Date_(2024, 3, 21)],
        parameterization="LOG_DISCOUNT", calibrate_discount_curve=False, target_overnight_route="USD.SOFR")
    market = dal.CurveInstrumentMarket_(routes=[dal.CurveScriptRoute_(id="USD.OIS", use_target_curve=True),
        dal.CurveScriptRoute_(id="USD.SOFR", projection=True, overnight=True)])
    return instrument, prepared, geometry, market, valuation


def run(mode):
    instrument, prepared, geometry, market, valuation = contract(mode)
    audit = dal.CompoundedOis_Get_ObservationSchedule(prepared)
    assert len(audit) == 4 and len(audit[0]) == 22
    print(f"\nObservation mode: {mode}\nFrozen schedule (zero-based rows, ACT/360 weights)")
    print(f"{'Row':<8}{'Actual start':<16}{'Rate start':<16}{'Weight':>16}  Source key")
    print("-" * 88)
    for row in prepared.observation_schedule:
        print(f"{row.row:<8}{str(row.accrual_start):<16}{str(row.rate_start):<16}"
              f"{format_float(row.compounding_accrual, 10):>16}  {row.source_key}")

    def fit(quote):
        observation = dal.CalibrationObservation_New("par", instrument, "ParRate",
            dal.CalibrationMarketQuote_New("q", quote, dal.CalibrationValueUnit_(kind="Rate")), 1e-10)
        spec = dal.CurveInstrumentCalibrationSpec_(geometry=geometry, market=market, valuation=valuation,
            compounded_ois=[prepared], observations=[observation])
        result = dal.Calibrate_CurveInstruments(spec, options=dal.CurveInstrumentCalibrationOptions_(jacobian_mode="AAD"))
        assert abs(result.raw_residuals[0]) <= 1e-12
        return spec, result

    def price(parameters):
        request = dal.CurveScriptEvaluationRequest_(geometry=geometry, market=market, valuation=valuation, parameters=parameters,
            compounded_ois=prepared, measure_ids=["Price"])
        result = dal.CurveScriptInstrument_Evaluate(instrument, request)
        assert len(dal.CurveScriptEvaluation_Get_Table(result, "observation_schedule")[0]) == 26
        return result.values[0]

    spec, calibrated = fit(0.025)
    results = []
    for pv_only in (False, True):
        accuracy = dal.ScriptCurveQuoteRiskAccuracy_(residual_absolute_limits=[1e-12], min_reciprocal_condition=1e-10,
            response_backward_error_limit=1e-12, pv_only=pv_only)
        snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(calibrated, accuracy)
        assert snapshot.risk_available and snapshot.mapping_available != pv_only
        result = dal.CalibrationInstrument_ValueWithQuoteRisk(instrument, snapshot, spec)
        assert result.available
        results.append(result)
    assert results[0].raw_sensitivity == results[1].raw_sensitivity
    reference = 0.0
    for step in (1e-4, 5e-5):
        _, up = fit(0.025 + step)
        _, down = fit(0.025 - step)
        reference = (price(up.parameters) - price(down.parameters)) / (2 * step)
        assert abs(results[0].raw_sensitivity[0] - reference) / 100 <= 1e-7 + 1e-5 * abs(reference / 100)
    assert results[0].dv01 == [value * 1e-4 for value in results[0].raw_sensitivity]
    print("\nEXACT calibration: q=0.025, fixed K=0.02, N=100 USD")
    print(f"Fitted ParRate={format_float(calibrated.model_measures[0], 10)}  "
          f"residual={format_float(calibrated.raw_residuals[0])}  theta={format_float(calibrated.parameters[0], 10)}")
    rejected = dal.CalibrationInstrument_ValueWithQuoteRisk(instrument, snapshot, spec,
        request=dal.ScriptCurveQuoteRiskRequest_(live_quote_payoff=True))
    assert not rejected.available
    assert rejected.diagnostics[0].code == "ScriptCurveQuoteRiskLiveQuoteUnsupported"
    return results[0], rejected, reference


def main():
    for mode in ("None", "Lookback", "ObservationShift", "Lockout"):
        result, rejected, reference = run(mode)
        gc.collect()
        print("\nOwning results after calibration inputs leave scope")
        print(f"{'PV (USD)':>18}{'Raw USD/decimal rate':>26}{'Signed DV01 USD/+1bp':>26}{'Recalibration FD':>24}")
        print("-" * 94)
        print(f"{format_float(result.pv, 10):>18}{format_float(result.raw_sensitivity[0], 10):>26}"
              f"{format_float(result.dv01[0], 10):>26}{format_float(reference, 10):>24}")
        for diagnostic in rejected.diagnostics:
            print(f"{diagnostic.code} / {diagnostic.field} / {diagnostic.message}")


if __name__ == "__main__":
    main()
