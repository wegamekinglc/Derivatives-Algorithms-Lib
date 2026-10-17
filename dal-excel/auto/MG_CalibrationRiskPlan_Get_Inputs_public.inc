

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskPlan_Get_Inputs
    (const OPER_* xl_plan, const OPER_* xl_complete)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskPlan_Get_Inputs");        
        argName = "plan (input #1)";
        const Handle_<StorableCalibrationRiskPlan_> plan = Excel::ToHandle<StorableCalibrationRiskPlan_>(_env, xl_plan);
        argName = "complete"; Excel::ValidateRiskRequestBoolean(xl_complete, "CalibrationRiskPlan_Get_Inputs; complete"); xl_complete = Excel::ScriptScalarInput(xl_complete);
        argName = "complete (input #2)";
        const bool complete = Excel::ToBool(xl_complete, true).value_or(false);
        argName = 0;
		Matrix_<Cell_> inputs;
        CalibrationRiskPlan_Get_Inputs(plan, complete, &inputs);
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

struct XlRegister_CalibrationRiskPlan_Get_Inputs_
{
    XlRegister_CalibrationRiskPlan_Get_Inputs_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Quote plan");
        argHelp.push_back("True returns every quote; default returns selected quotes in requested order");
        Excel::Register("Base", "xl_CalibrationRiskPlan_Get_Inputs", "CALIBRATIONRISKPLAN.GET.INPUTS", "Copy complete or selected canonical quote coordinates", "QQQ", "plan,[complete]", argHelp, false);
    }
};
static XlRegister_CalibrationRiskPlan_Get_Inputs_ The_CalibrationRiskPlan_Get_Inputs_XlRegisterer;
