

extern "C" __declspec(dllexport) OPER_* xl_GSRSLVQuoteRiskResult_Get
    (const OPER_* xl_result, const OPER_* xl_attribute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRSLVQuoteRiskResult_Get");        
        argName = "result (input #1)";
        const Handle_<StorableGSRSLVQuoteRiskResult_> result = Excel::ToHandle<StorableGSRSLVQuoteRiskResult_>(_env, xl_result);
        argName = "attribute (input #2)";
        const String_ attribute = Excel::ToString(xl_attribute);
        argName = 0;
		Matrix_<Cell_> value;
        GSRSLVQuoteRiskResult_Get(result, attribute, &value);
        Excel::Retval_ retval;
        retval.Load(value);
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

struct XlRegister_GSRSLVQuoteRiskResult_Get_
{
    XlRegister_GSRSLVQuoteRiskResult_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Quote risk result");
        argHelp.push_back("prices, quoteNames, quoteUnits, sensitivities, refinementErrors, stable, activeSetStable or curveRiskIncluded");
        Excel::Register("Base", "xl_GSRSLVQuoteRiskResult_Get", "GSRSLVQUOTERISKRESULT.GET", "Read a calibrated SLV quote risk diagnostic", "QQQ", "result,attribute", argHelp, false);
    }
};
static XlRegister_GSRSLVQuoteRiskResult_Get_ The_GSRSLVQuoteRiskResult_Get_XlRegisterer;
