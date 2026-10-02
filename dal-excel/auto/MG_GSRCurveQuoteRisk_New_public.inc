

extern "C" __declspec(dllexport) OPER_* xl_GSRCurveQuoteRisk_New
    (const OPER_* xl_snapshot, const OPER_* xl_market, const OPER_* xl_provenance, const OPER_* xl_discountComponent, const OPER_* xl_projectionComponents)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRCurveQuoteRisk_New");        
        argName = "snapshot (input #1)";
        const Handle_<GSRCurveData_> snapshot = Excel::ToHandle<GSRCurveData_>(_env, xl_snapshot);
        argName = "market (input #2)";
        const Handle_<StorableRatePricingMarket_> market = Excel::ToHandle<StorableRatePricingMarket_>(_env, xl_market);
        argName = "provenance (input #3)";
        const Handle_<StorableRateQuoteRiskProvenance_> provenance = Excel::ToHandle<StorableRateQuoteRiskProvenance_>(_env, xl_provenance);
        argName = "discountComponent (input #4)";
        const String_ discountComponent = Excel::ToString(xl_discountComponent);
        argName = "projectionComponents (input #5)";
        const Vector_<String_> projectionComponents = Excel::ToStringVector(xl_projectionComponents, true);
        argName = 0;
		Handle_<StorableGSRCurveQuoteRisk_> result;
        GSRCurveQuoteRisk_New(snapshot, market, provenance, discountComponent, projectionComponents, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_GSRCurveQuoteRisk_New_
{
    XlRegister_GSRCurveQuoteRisk_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Curve snapshot matching the bound market");
        argHelp.push_back("Bound curve market");
        argHelp.push_back("Available curve calibration provenance");
        argHelp.push_back("Discount component key");
        argHelp.push_back("One component key per snapshot projection tenor");
        Excel::Register("Base", "xl_GSRCurveQuoteRisk_New", "GSRCURVEQUOTERISK.NEW", "Connect a GSR curve snapshot to validated market quote provenance", "QQQQQQ", "snapshot,market,provenance,discountComponent,[projectionComponents]", argHelp, false);
    }
};
static XlRegister_GSRCurveQuoteRisk_New_ The_GSRCurveQuoteRisk_New_XlRegisterer;
