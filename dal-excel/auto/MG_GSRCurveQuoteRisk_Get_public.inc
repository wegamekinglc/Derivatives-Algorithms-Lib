

extern "C" __declspec(dllexport) OPER_* xl_GSRCurveQuoteRisk_Get
    (const OPER_* xl_result, const OPER_* xl_attribute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRCurveQuoteRisk_Get");        
        argName = "result (input #1)";
        const Handle_<StorableGSRCurveQuoteRisk_> result = Excel::ToHandle<StorableGSRCurveQuoteRisk_>(_env, xl_result);
        argName = "attribute (input #2)";
        const String_ attribute = Excel::ToString(xl_attribute);
        argName = 0;
		Matrix_<Cell_> value;
        GSRCurveQuoteRisk_Get(result, attribute, &value);
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

struct XlRegister_GSRCurveQuoteRisk_Get_
{
    XlRegister_GSRCurveQuoteRisk_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Curve quote bridge");
        argHelp.push_back("quoteNames, quoteUnits or logDFQuoteJacobian");
        Excel::Register("Base", "xl_GSRCurveQuoteRisk_Get", "GSRCURVEQUOTERISK.GET", "Read a GSR curve quote bridge diagnostic", "QQQ", "result,attribute", argHelp, false);
    }
};
static XlRegister_GSRCurveQuoteRisk_Get_ The_GSRCurveQuoteRisk_Get_XlRegisterer;
