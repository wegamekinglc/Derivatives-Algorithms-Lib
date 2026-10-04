

extern "C" __declspec(dllexport) OPER_* xl_RiskResult_Get_Values
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RiskResult_Get_Values");        
        argName = "result (input #1)";
        const Handle_<StorableRiskResult_> result = Excel::ToHandle<StorableRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> values;
        RiskResult_Get_Values(result, &values);
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

struct XlRegister_RiskResult_Get_Values_
{
    XlRegister_RiskResult_Get_Values_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_RiskResult_Get_Values", "RISKRESULT.GET.VALUES", "Extract mean numeric output values", "QQ", "result", argHelp, false);
    }
};
static XlRegister_RiskResult_Get_Values_ The_RiskResult_Get_Values_XlRegisterer;
