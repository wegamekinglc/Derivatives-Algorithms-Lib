

extern "C" __declspec(dllexport) OPER_* xl_RiskResult_Get_LegacyValues
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RiskResult_Get_LegacyValues");        
        argName = "result (input #1)";
        const Handle_<StorableRiskResult_> result = Excel::ToHandle<StorableRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> values;
        RiskResult_Get_LegacyValues(result, &values);
        Excel::Retval_ retval;
        retval.Load(values);
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

struct XlRegister_RiskResult_Get_LegacyValues_
{
    XlRegister_RiskResult_Get_LegacyValues_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_RiskResult_Get_LegacyValues", "RISKRESULT.GET.LEGACYVALUES", "Extract raw PV and d_label compatibility keys", "QQ", "result", argHelp, false);
    }
};
static XlRegister_RiskResult_Get_LegacyValues_ The_RiskResult_Get_LegacyValues_XlRegisterer;
