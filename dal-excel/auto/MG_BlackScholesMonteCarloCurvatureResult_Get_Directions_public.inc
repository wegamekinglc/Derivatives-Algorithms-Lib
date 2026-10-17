

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloCurvatureResult_Get_Directions
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloCurvatureResult_Get_Directions");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesMonteCarloCurvatureResult_> result = Excel::ToHandle<StorableBlackScholesMonteCarloCurvatureResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Directions; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		Matrix_<Cell_> directions;
        BlackScholesMonteCarloCurvatureResult_Get_Directions(result, &directions);
        Excel::Retval_ retval;
        retval.Load(directions);
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

struct XlRegister_BlackScholesMonteCarloCurvatureResult_Get_Directions_
{
    XlRegister_BlackScholesMonteCarloCurvatureResult_Get_Directions_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning finite-step result");
        Excel::Register("Base", "xl_BlackScholesMonteCarloCurvatureResult_Get_Directions", "BLACKSCHOLESMONTECARLOCURVATURERESULT.GET.DIRECTIONS", "Copy stored native MC curvature directions", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloCurvatureResult_Get_Directions_ The_BlackScholesMonteCarloCurvatureResult_Get_Directions_XlRegisterer;
