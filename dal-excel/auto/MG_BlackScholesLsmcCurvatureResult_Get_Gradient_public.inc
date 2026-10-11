

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcCurvatureResult_Get_Gradient
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcCurvatureResult_Get_Gradient");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesLsmcCurvatureResult_> result = Excel::ToHandle<StorableBlackScholesLsmcCurvatureResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Gradient; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		Matrix_<Cell_> gradient;
        BlackScholesLsmcCurvatureResult_Get_Gradient(result, &gradient);
        Excel::Retval_ retval;
        retval.Load(gradient);
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

struct XlRegister_BlackScholesLsmcCurvatureResult_Get_Gradient_
{
    XlRegister_BlackScholesLsmcCurvatureResult_Get_Gradient_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable result");
        Excel::Register("Base", "xl_BlackScholesLsmcCurvatureResult_Get_Gradient", "BLACKSCHOLESLSMCCURVATURERESULT.GET.GRADIENT", "Copy BlackScholesLsmcCurvatureResult Get Gradient", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcCurvatureResult_Get_Gradient_ The_BlackScholesLsmcCurvatureResult_Get_Gradient_XlRegisterer;
