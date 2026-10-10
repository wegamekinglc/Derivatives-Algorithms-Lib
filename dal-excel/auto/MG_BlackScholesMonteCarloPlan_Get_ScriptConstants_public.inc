

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloPlan_Get_ScriptConstants
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloPlan_Get_ScriptConstants");        
        argName = "plan (input #1)";
        const Handle_<StorableBlackScholesMonteCarloPlan_> plan = Excel::ToHandle<StorableBlackScholesMonteCarloPlan_>(_env, xl_plan);
        argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloPlan_Get_ScriptConstants; plan"); xl_plan = Excel::ScriptScalarInput(xl_plan);
        argName = 0;
		Matrix_<Cell_> scriptconstants;
        BlackScholesMonteCarloPlan_Get_ScriptConstants(plan, &scriptconstants);
        Excel::Retval_ retval;
        retval.Load(scriptconstants);
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

struct XlRegister_BlackScholesMonteCarloPlan_Get_ScriptConstants_
{
    XlRegister_BlackScholesMonteCarloPlan_Get_ScriptConstants_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Sealed native Black-Scholes script plan");
        Excel::Register("Base", "xl_BlackScholesMonteCarloPlan_Get_ScriptConstants", "BLACKSCHOLESMONTECARLOPLAN.GET.SCRIPTCONSTANTS", "Copy sealed Monte Carlo plan scriptconstants", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloPlan_Get_ScriptConstants_ The_BlackScholesMonteCarloPlan_Get_ScriptConstants_XlRegisterer;
