

extern "C" __declspec(dllexport) OPER_* xl_StagedMultiCurveQuoteRiskProvenance_New
    (const OPER_* xl_result, const OPER_* xl_calibrationId, const OPER_* xl_parameterBlockKeys, const OPER_* xl_componentKeys, const OPER_* xl_market, const OPER_* xl_retainCalibrationRecord)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("StagedMultiCurveQuoteRiskProvenance_New");        
        argName = "result (input #1)";
        const Handle_<StorableMultiCurveCalibrationResult_> result = Excel::ToHandle<StorableMultiCurveCalibrationResult_>(_env, xl_result);
        argName = "calibrationId (input #2)";
        const String_ calibrationId = Excel::ToString(xl_calibrationId);
        argName = "parameterBlockKeys (input #3)";
        const Vector_<String_> parameterBlockKeys = Excel::ToStringVector(xl_parameterBlockKeys);
        argName = "componentKeys (input #4)";
        const Vector_<String_> componentKeys = Excel::ToStringVector(xl_componentKeys);
        argName = "market (input #5)";
        const Handle_<StorableRatePricingMarket_> market = Excel::ToHandle<StorableRatePricingMarket_>(_env, xl_market);
        argName = "retainCalibrationRecord"; xl_retainCalibrationRecord = Excel::ScriptScalarInput(xl_retainCalibrationRecord); Excel::ValidateCalibrationRecordCaptureInput(xl_retainCalibrationRecord);
        argName = "retainCalibrationRecord (input #6)";
        const Cell_ retainCalibrationRecord = Excel::ToCell(xl_retainCalibrationRecord, true);
        argName = 0;
		Handle_<StorableRateQuoteRiskProvenance_> provenance;
        StagedMultiCurveQuoteRiskProvenance_New(result, calibrationId, parameterBlockKeys, componentKeys, market, retainCalibrationRecord, &provenance);
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

struct XlRegister_StagedMultiCurveQuoteRiskProvenance_New_
{
    XlRegister_StagedMultiCurveQuoteRiskProvenance_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The result of CALIBRATE.MULTICURVE with recordQuoteRisk enabled");
        argHelp.push_back("Identifier used to join and order quote-risk rows");
        argHelp.push_back("Original stage keys stage:0 and stage:1");
        argHelp.push_back("Corresponding discount and forecast component keys in the market");
        argHelp.push_back("The pricing market with the fitted curves, valuation time and fixings");
        argHelp.push_back("TRUE retains the canonical provenance state for inspection");
        Excel::Register("Base", "xl_StagedMultiCurveQuoteRiskProvenance_New", "STAGEDMULTICURVEQUOTERISKPROVENANCE.NEW", "Bind sequential discount and forward quote risk to a pricing market", "QQQQQQQ", "result,calibrationId,parameterBlockKeys,componentKeys,market,[retainCalibrationRecord]", argHelp, false);
    }
};
static XlRegister_StagedMultiCurveQuoteRiskProvenance_New_ The_StagedMultiCurveQuoteRiskProvenance_New_XlRegisterer;
