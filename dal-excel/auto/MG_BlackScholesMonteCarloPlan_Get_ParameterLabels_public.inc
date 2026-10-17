

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloPlan_Get_ParameterLabels
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloPlan_Get_ParameterLabels");        
        argName = "plan (input #1)";
        const Handle_<StorableBlackScholesMonteCarloPlan_> plan = Excel::ToHandle<StorableBlackScholesMonteCarloPlan_>(_env, xl_plan);
        argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloPlan_Get_ParameterLabels; plan"); xl_plan = Excel::ScriptScalarInput(xl_plan);
        argName = 0;
		Vector_<String_> parameterlabels;
        BlackScholesMonteCarloPlan_Get_ParameterLabels(plan, &parameterlabels);
        Excel::Retval_ retval;
        retval.Load(parameterlabels);
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

struct XlRegister_BlackScholesMonteCarloPlan_Get_ParameterLabels_
{
    XlRegister_BlackScholesMonteCarloPlan_Get_ParameterLabels_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Sealed native Black-Scholes script plan");
        Excel::Register("Base", "xl_BlackScholesMonteCarloPlan_Get_ParameterLabels", "BLACKSCHOLESMONTECARLOPLAN.GET.PARAMETERLABELS", "Copy sealed Monte Carlo plan parameterlabels", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloPlan_Get_ParameterLabels_ The_BlackScholesMonteCarloPlan_Get_ParameterLabels_XlRegisterer;
