

extern "C" __declspec(dllexport) OPER_* xl_CompoundedOisParRateRiskResult_Get_Diagnostics
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CompoundedOisParRateRiskResult_Get_Diagnostics");        
        argName = "result (input #1)";
        const Handle_<StorableCompoundedOisParRateRiskResult_> result = Excel::ToHandle<StorableCompoundedOisParRateRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> value;
        CompoundedOisParRateRiskResult_Get_Diagnostics(result, &value);
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

struct XlRegister_CompoundedOisParRateRiskResult_Get_Diagnostics_
{
    XlRegister_CompoundedOisParRateRiskResult_Get_Diagnostics_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning par-rate risk result");
        Excel::Register("Base", "xl_CompoundedOisParRateRiskResult_Get_Diagnostics", "COMPOUNDEDOISPARRATERISKRESULT.GET.DIAGNOSTICS", "Read structured par-rate diagnostics", "QQ", "result", argHelp, false);
    }
};
static XlRegister_CompoundedOisParRateRiskResult_Get_Diagnostics_ The_CompoundedOisParRateRiskResult_Get_Diagnostics_XlRegisterer;
