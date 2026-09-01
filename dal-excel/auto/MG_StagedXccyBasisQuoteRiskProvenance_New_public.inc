

extern "C" __declspec(dllexport) OPER_* xl_StagedXccyBasisQuoteRiskProvenance_New
    (const OPER_* xl_result, const OPER_* xl_calibrationId, const OPER_* xl_parameterBlockKeys, const OPER_* xl_componentKeys, const OPER_* xl_market)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("StagedXccyBasisQuoteRiskProvenance_New");        
        argName = "result (input #1)";
        const Handle_<StorableCrossCurrencyCalibrationResult_> result = Excel::ToHandle<StorableCrossCurrencyCalibrationResult_>(_env, xl_result);
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
        StagedXccyBasisQuoteRiskProvenance_New(result, calibrationId, parameterBlockKeys, componentKeys, market, &provenance);
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

struct XlRegister_StagedXccyBasisQuoteRiskProvenance_New_
{
    XlRegister_StagedXccyBasisQuoteRiskProvenance_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The exact result returned by CALIBRATE.XCCYMARKET");
        argHelp.push_back("Non-empty identifier used to join and order quote-risk rows");
        argHelp.push_back("Staged basis parameter block key");
        argHelp.push_back("Pricing-market basis component key parallel to parameterBlockKeys");
        argHelp.push_back("The exact pricing market bound to the calibrated basis curve");
        Excel::Register("Base", "xl_StagedXccyBasisQuoteRiskProvenance_New", "STAGEDXCCYBASISQUOTERISKPROVENANCE.NEW", "Freeze quote-risk provenance from one staged XCCY basis calibration result", "QQQQQQ", "result,calibrationId,parameterBlockKeys,componentKeys,market", argHelp, false);
    }
};
static XlRegister_StagedXccyBasisQuoteRiskProvenance_New_ The_StagedXccyBasisQuoteRiskProvenance_New_XlRegisterer;
