

extern "C" __declspec(dllexport) OPER_* xl_RiskResult_Get_Outputs
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RiskResult_Get_Outputs");        
        argName = "result (input #1)";
        const Handle_<StorableRiskResult_> result = Excel::ToHandle<StorableRiskResult_>(_env, xl_result);
        argName = 0;
		Vector_<String_> outputs;
        RiskResult_Get_Outputs(result, &outputs);
        Excel::Retval_ retval;
        retval.Load(outputs);
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

struct XlRegister_RiskResult_Get_Outputs_
{
    XlRegister_RiskResult_Get_Outputs_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_RiskResult_Get_Outputs", "RISKRESULT.GET.OUTPUTS", "Extract ordered output IDs", "QQ", "result", argHelp, false);
    }
};
static XlRegister_RiskResult_Get_Outputs_ The_RiskResult_Get_Outputs_XlRegisterer;
