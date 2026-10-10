

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloCurvatureResult_Get_Base
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloCurvatureResult_Get_Base");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesMonteCarloCurvatureResult_> result = Excel::ToHandle<StorableBlackScholesMonteCarloCurvatureResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Base; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		Handle_<StorableBlackScholesMonteCarloResult_> base;
        BlackScholesMonteCarloCurvatureResult_Get_Base(result, &base);
        Excel::Retval_ retval;
        retval.Load(_env, base);
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

struct XlRegister_BlackScholesMonteCarloCurvatureResult_Get_Base_
{
    XlRegister_BlackScholesMonteCarloCurvatureResult_Get_Base_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning finite-step result");
        Excel::Register("Base", "xl_BlackScholesMonteCarloCurvatureResult_Get_Base", "BLACKSCHOLESMONTECARLOCURVATURERESULT.GET.BASE", "Copy stored native MC curvature base", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloCurvatureResult_Get_Base_ The_BlackScholesMonteCarloCurvatureResult_Get_Base_XlRegisterer;
