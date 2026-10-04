

extern "C" __declspec(dllexport) OPER_* xl_RiskResult_Get_Inputs
    (const OPER_* xl_result, const OPER_* xl_complete)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RiskResult_Get_Inputs");        
        argName = "result (input #1)";
        const Handle_<StorableRiskResult_> result = Excel::ToHandle<StorableRiskResult_>(_env, xl_result);
        argName = "complete (input #2)";
        const bool complete = Excel::ToBool(xl_complete, true).value_or(false);
        argName = 0;
		Matrix_<Cell_> inputs;
        RiskResult_Get_Inputs(result, complete, &inputs);
        Excel::Retval_ retval;
        retval.Load(inputs);
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

struct XlRegister_RiskResult_Get_Inputs_
{
    XlRegister_RiskResult_Get_Inputs_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        argHelp.push_back("False returns selected columns; true returns the full original axis");
        Excel::Register("Base", "xl_RiskResult_Get_Inputs", "RISKRESULT.GET.INPUTS", "Extract ordered coordinate definitions", "QQQ", "result,[complete]", argHelp, false);
    }
};
static XlRegister_RiskResult_Get_Inputs_ The_RiskResult_Get_Inputs_XlRegisterer;
