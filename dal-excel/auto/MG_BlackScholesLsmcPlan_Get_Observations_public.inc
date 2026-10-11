

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcPlan_Get_Observations
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcPlan_Get_Observations");        
        argName = "plan (input #1)";
        const Handle_<StorableBlackScholesLsmcPlan_> plan = Excel::ToHandle<StorableBlackScholesLsmcPlan_>(_env, xl_plan);
        argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_Observations; plan"); xl_plan = Excel::ScriptScalarInput(xl_plan);
        argName = 0;
		Matrix_<Cell_> observations;
        BlackScholesLsmcPlan_Get_Observations(plan, &observations);
        Excel::Retval_ retval;
        retval.Load(observations);
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

struct XlRegister_BlackScholesLsmcPlan_Get_Observations_
{
    XlRegister_BlackScholesLsmcPlan_Get_Observations_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable plan");
        Excel::Register("Base", "xl_BlackScholesLsmcPlan_Get_Observations", "BLACKSCHOLESLSMCPLAN.GET.OBSERVATIONS", "Copy BlackScholesLsmcPlan Get Observations", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcPlan_Get_Observations_ The_BlackScholesLsmcPlan_Get_Observations_XlRegisterer;
