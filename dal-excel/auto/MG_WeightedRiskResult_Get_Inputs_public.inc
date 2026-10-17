

extern "C" __declspec(dllexport) OPER_* xl_WeightedRiskResult_Get_Inputs
    (const OPER_* xl_result, const OPER_* xl_complete)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("WeightedRiskResult_Get_Inputs");        
        argName = "result (input #1)";
        const Handle_<StorableWeightedRiskResult_> result = Excel::ToHandle<StorableWeightedRiskResult_>(_env, xl_result);
        argName = "complete"; xl_complete = Excel::ScriptScalarInput(xl_complete); Excel::ValidateRiskRequestBoolean(xl_complete, "WeightedRiskResult_Get_Inputs; complete");
        argName = "complete (input #2)";
        const bool complete = Excel::ToBool(xl_complete, true).value_or(false);
        argName = 0;
		Matrix_<Cell_> inputs;
        WeightedRiskResult_Get_Inputs(result, complete, &inputs);
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

struct XlRegister_WeightedRiskResult_Get_Inputs_
{
    XlRegister_WeightedRiskResult_Get_Inputs_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        argHelp.push_back("True returns the full original input axis");
        Excel::Register("Base", "xl_WeightedRiskResult_Get_Inputs", "WEIGHTEDRISKRESULT.GET.INPUTS", "Extract ordered selected or complete input coordinates", "QQQ", "result,[complete]", argHelp, false);
    }
};
static XlRegister_WeightedRiskResult_Get_Inputs_ The_WeightedRiskResult_Get_Inputs_XlRegisterer;
