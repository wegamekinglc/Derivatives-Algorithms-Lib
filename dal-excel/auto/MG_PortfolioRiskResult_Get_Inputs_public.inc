

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_Inputs
    (const OPER_* xl_result, const OPER_* xl_complete)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_Inputs");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        xl_complete = Excel::ScriptScalarInput(xl_complete); argName = "complete"; Excel::ValidateRiskRequestBoolean(xl_complete, "PortfolioRiskResult_Get_Inputs; complete");
        argName = "complete (input #2)";
        const bool complete = Excel::ToBool(xl_complete, true).value_or(false);
        argName = 0;
		Matrix_<Cell_> inputs;
        PortfolioRiskResult_Get_Inputs(result, complete, &inputs);
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

struct XlRegister_PortfolioRiskResult_Get_Inputs_
{
    XlRegister_PortfolioRiskResult_Get_Inputs_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted or attribution portfolio result");
        argHelp.push_back("True selects the original complete owner/private catalog");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_Inputs", "PORTFOLIORISKRESULT.GET.INPUTS", "Extract selected or complete global portfolio input coordinates", "QQQ", "result,[complete]", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_Inputs_ The_PortfolioRiskResult_Get_Inputs_XlRegisterer;
