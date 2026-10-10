

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloPlan_Get_Valuation
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloPlan_Get_Valuation");        
        argName = "plan (input #1)";
        const Handle_<StorableBlackScholesMonteCarloPlan_> plan = Excel::ToHandle<StorableBlackScholesMonteCarloPlan_>(_env, xl_plan);
        argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloPlan_Get_Valuation; plan"); xl_plan = Excel::ScriptScalarInput(xl_plan);
        argName = 0;
		Handle_<StorableScriptValuationSettings_> valuation;
        BlackScholesMonteCarloPlan_Get_Valuation(plan, &valuation);
        Excel::Retval_ retval;
        retval.Load(_env, valuation);
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

struct XlRegister_BlackScholesMonteCarloPlan_Get_Valuation_
{
    XlRegister_BlackScholesMonteCarloPlan_Get_Valuation_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Sealed native Black-Scholes script plan");
        Excel::Register("Base", "xl_BlackScholesMonteCarloPlan_Get_Valuation", "BLACKSCHOLESMONTECARLOPLAN.GET.VALUATION", "Copy sealed Monte Carlo plan valuation", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloPlan_Get_Valuation_ The_BlackScholesMonteCarloPlan_Get_Valuation_XlRegisterer;
