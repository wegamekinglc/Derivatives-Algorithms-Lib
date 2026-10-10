

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloCurvatureResult_Get_Plan
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloCurvatureResult_Get_Plan");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesMonteCarloCurvatureResult_> result = Excel::ToHandle<StorableBlackScholesMonteCarloCurvatureResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Plan; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		Handle_<StorableBlackScholesMonteCarloPlan_> plan;
        BlackScholesMonteCarloCurvatureResult_Get_Plan(result, &plan);
        Excel::Retval_ retval;
        retval.Load(_env, plan);
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

struct XlRegister_BlackScholesMonteCarloCurvatureResult_Get_Plan_
{
    XlRegister_BlackScholesMonteCarloCurvatureResult_Get_Plan_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning finite-step result");
        Excel::Register("Base", "xl_BlackScholesMonteCarloCurvatureResult_Get_Plan", "BLACKSCHOLESMONTECARLOCURVATURERESULT.GET.PLAN", "Copy stored native MC curvature plan", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloCurvatureResult_Get_Plan_ The_BlackScholesMonteCarloCurvatureResult_Get_Plan_XlRegisterer;
