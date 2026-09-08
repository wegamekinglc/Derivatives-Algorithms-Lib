

extern "C" __declspec(dllexport) OPER_* xl_JointMultiCurveQuoteRiskProvenance_New
    (const OPER_* xl_result, const OPER_* xl_calibrationId, const OPER_* xl_parameterBlockKeys, const OPER_* xl_componentKeys, const OPER_* xl_market)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JointMultiCurveQuoteRiskProvenance_New");        
        argName = "result (input #1)";
        const Handle_<StorableJointMultiCurveCalibrationResult_> result = Excel::ToHandle<StorableJointMultiCurveCalibrationResult_>(_env, xl_result);
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
        JointMultiCurveQuoteRiskProvenance_New(result, calibrationId, parameterBlockKeys, componentKeys, market, &provenance);
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

struct XlRegister_JointMultiCurveQuoteRiskProvenance_New_
{
    XlRegister_JointMultiCurveQuoteRiskProvenance_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The result returned by CALIBRATE.JOINTMULTICURVE with computeEffJacobianInverse enabled");
        argHelp.push_back("Non-empty identifier used to join and order quote-risk rows");
        argHelp.push_back("Ordered declaration keys: curve:0, curve:1, and so on");
        argHelp.push_back("Pricing-market component keys parallel to parameterBlockKeys");
        argHelp.push_back("The pricing market bound to all calibrated blocks, valuation time and fixings");
        Excel::Register("Base", "xl_JointMultiCurveQuoteRiskProvenance_New", "JOINTMULTICURVEQUOTERISKPROVENANCE.NEW", "Freeze v2 quote-risk provenance from an exact generic joint calibration result", "QQQQQQ", "result,calibrationId,parameterBlockKeys,componentKeys,market", argHelp, false);
    }
};
static XlRegister_JointMultiCurveQuoteRiskProvenance_New_ The_JointMultiCurveQuoteRiskProvenance_New_XlRegisterer;
