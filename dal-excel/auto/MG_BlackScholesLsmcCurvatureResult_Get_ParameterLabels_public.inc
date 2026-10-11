

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcCurvatureResult_Get_ParameterLabels
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcCurvatureResult_Get_ParameterLabels");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesLsmcCurvatureResult_> result = Excel::ToHandle<StorableBlackScholesLsmcCurvatureResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_ParameterLabels; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		Vector_<String_> parameterlabels;
        BlackScholesLsmcCurvatureResult_Get_ParameterLabels(result, &parameterlabels);
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

struct XlRegister_BlackScholesLsmcCurvatureResult_Get_ParameterLabels_
{
    XlRegister_BlackScholesLsmcCurvatureResult_Get_ParameterLabels_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable result");
        Excel::Register("Base", "xl_BlackScholesLsmcCurvatureResult_Get_ParameterLabels", "BLACKSCHOLESLSMCCURVATURERESULT.GET.PARAMETERLABELS", "Copy BlackScholesLsmcCurvatureResult Get ParameterLabels", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcCurvatureResult_Get_ParameterLabels_ The_BlackScholesLsmcCurvatureResult_Get_ParameterLabels_XlRegisterer;
