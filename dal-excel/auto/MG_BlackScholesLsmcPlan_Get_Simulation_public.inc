

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcPlan_Get_Simulation
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcPlan_Get_Simulation");        
        argName = "plan (input #1)";
        const Handle_<StorableBlackScholesLsmcPlan_> plan = Excel::ToHandle<StorableBlackScholesLsmcPlan_>(_env, xl_plan);
        argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_Simulation; plan"); xl_plan = Excel::ScriptScalarInput(xl_plan);
        argName = 0;
		Handle_<StorableMonteCarloSettings_> simulation;
        BlackScholesLsmcPlan_Get_Simulation(plan, &simulation);
        Excel::Retval_ retval;
        retval.Load(_env, simulation);
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

struct XlRegister_BlackScholesLsmcPlan_Get_Simulation_
{
    XlRegister_BlackScholesLsmcPlan_Get_Simulation_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable plan");
        Excel::Register("Base", "xl_BlackScholesLsmcPlan_Get_Simulation", "BLACKSCHOLESLSMCPLAN.GET.SIMULATION", "Copy BlackScholesLsmcPlan Get Simulation", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcPlan_Get_Simulation_ The_BlackScholesLsmcPlan_Get_Simulation_XlRegisterer;
