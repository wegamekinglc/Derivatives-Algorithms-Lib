

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskPlan_Get_Inputs
    (const OPER_* xl_plan, const OPER_* xl_complete)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskPlan_Get_Inputs");        
        argName = "plan (input #1)";
        const Handle_<StorableDupireScriptRiskPlan_> plan = Excel::ToHandle<StorableDupireScriptRiskPlan_>(_env, xl_plan);
        argName = "complete"; Excel::ValidateRiskRequestBoolean(xl_complete, "DupireScriptRiskPlan_Get_Inputs; complete"); xl_complete = Excel::ScriptScalarInput(xl_complete);
        argName = "complete (input #2)";
        const bool complete = Excel::ToBool(xl_complete, true).value_or(false);
        argName = 0;
		Matrix_<Cell_> inputs;
        DupireScriptRiskPlan_Get_Inputs(plan, complete, &inputs);
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

struct XlRegister_DupireScriptRiskPlan_Get_Inputs_
{
    XlRegister_DupireScriptRiskPlan_Get_Inputs_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Sealed plan");
        argHelp.push_back("True returns the complete model axis; default returns required inputs");
        Excel::Register("Base", "xl_DupireScriptRiskPlan_Get_Inputs", "DUPIRESCRIPTRISKPLAN.GET.INPUTS", "Copy required or complete native model and constant axes", "QQQ", "plan,[complete]", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskPlan_Get_Inputs_ The_DupireScriptRiskPlan_Get_Inputs_XlRegisterer;
