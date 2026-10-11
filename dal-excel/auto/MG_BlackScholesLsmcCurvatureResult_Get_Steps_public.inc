

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcCurvatureResult_Get_Steps
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcCurvatureResult_Get_Steps");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesLsmcCurvatureResult_> result = Excel::ToHandle<StorableBlackScholesLsmcCurvatureResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Steps; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		Matrix_<Cell_> steps;
        BlackScholesLsmcCurvatureResult_Get_Steps(result, &steps);
        Excel::Retval_ retval;
        retval.Load(steps);
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

struct XlRegister_BlackScholesLsmcCurvatureResult_Get_Steps_
{
    XlRegister_BlackScholesLsmcCurvatureResult_Get_Steps_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable result");
        Excel::Register("Base", "xl_BlackScholesLsmcCurvatureResult_Get_Steps", "BLACKSCHOLESLSMCCURVATURERESULT.GET.STEPS", "Copy BlackScholesLsmcCurvatureResult Get Steps", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcCurvatureResult_Get_Steps_ The_BlackScholesLsmcCurvatureResult_Get_Steps_XlRegisterer;
