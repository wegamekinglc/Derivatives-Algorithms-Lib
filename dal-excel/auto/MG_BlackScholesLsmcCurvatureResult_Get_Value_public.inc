

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcCurvatureResult_Get_Value
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcCurvatureResult_Get_Value");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesLsmcCurvatureResult_> result = Excel::ToHandle<StorableBlackScholesLsmcCurvatureResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Value; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		double value;
        BlackScholesLsmcCurvatureResult_Get_Value(result, &value);
        Excel::Retval_ retval;
        retval.Load(value);
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

struct XlRegister_BlackScholesLsmcCurvatureResult_Get_Value_
{
    XlRegister_BlackScholesLsmcCurvatureResult_Get_Value_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable result");
        Excel::Register("Base", "xl_BlackScholesLsmcCurvatureResult_Get_Value", "BLACKSCHOLESLSMCCURVATURERESULT.GET.VALUE", "Copy BlackScholesLsmcCurvatureResult Get Value", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcCurvatureResult_Get_Value_ The_BlackScholesLsmcCurvatureResult_Get_Value_XlRegisterer;
