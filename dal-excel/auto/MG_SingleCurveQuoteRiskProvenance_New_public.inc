

extern "C" __declspec(dllexport) OPER_* xl_SingleCurveQuoteRiskProvenance_New
    (const OPER_* xl_result, const OPER_* xl_calibrationId, const OPER_* xl_parameterBlockKeys, const OPER_* xl_componentKeys, const OPER_* xl_market)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("SingleCurveQuoteRiskProvenance_New");        
        argName = "result (input #1)";
        const Handle_<StorableCurveCalibrationResult_> result = Excel::ToHandle<StorableCurveCalibrationResult_>(_env, xl_result);
        argName = "calibrationId (input #2)";
        const String_ calibrationId = Excel::ToString(xl_calibrationId);
        argName = "parameterBlockKeys (input #3)";
        const Vector_<String_> parameterBlockKeys = Excel::ToStringVector(xl_parameterBlockKeys);
        argName = "componentKeys (input #4)";
        const Vector_<String_> componentKeys = Excel::ToStringVector(xl_componentKeys);
        argName = "market (input #5)";
        const Handle_<StorableRatePricingMarket_> market = Excel::ToHandle<StorableRatePricingMarket_>(_env, xl_market);
        argName = 0;
		Handle_<StorableRateQuoteRiskProvenance_> provenance;
        SingleCurveQuoteRiskProvenance_New(result, calibrationId, parameterBlockKeys, componentKeys, market, &provenance);
        Excel::Retval_ retval;
        retval.Load(_env, provenance);
        return retval.ToXloper();
    }
    catch (std::exception& e)
    {
        return Excel::Error(e.what(), argName);
    }
    catch (...)
    {
        return Excel::Error("Unknown error", argName);
    }
}

struct XlRegister_SingleCurveQuoteRiskProvenance_New_
{
    XlRegister_SingleCurveQuoteRiskProvenance_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The exact result returned by CALIBRATE.SINGLECURVE");
        argHelp.push_back("Non-empty identifier used to join and order quote-risk rows");
        argHelp.push_back("Calibration parameter block keys, in calibration order");
        argHelp.push_back("Pricing-market component keys parallel to parameterBlockKeys");
        argHelp.push_back("The exact pricing market bound to the calibrated curve");
        Excel::Register("Base", "xl_SingleCurveQuoteRiskProvenance_New", "SINGLECURVEQUOTERISKPROVENANCE.NEW", "Freeze quote-risk provenance from one single-curve calibration result", "QQQQQQ", "result,calibrationId,parameterBlockKeys,componentKeys,market", argHelp, false);
    }
};
static XlRegister_SingleCurveQuoteRiskProvenance_New_ The_SingleCurveQuoteRiskProvenance_New_XlRegisterer;
