

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcCurvatureResult_Get_BasePolicy
    (const OPER_* xl_result, const OPER_* xl_index)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcCurvatureResult_Get_BasePolicy");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesLsmcCurvatureResult_> result = Excel::ToHandle<StorableBlackScholesLsmcCurvatureResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_BasePolicy; result"); xl_result = Excel::ScriptScalarInput(xl_result); argName = "index"; Excel::ValidateCurvatureNumber(*Excel::ScriptScalarInput(xl_index), "BlackScholesLsmcCurvatureResult_Get_BasePolicy; index; "); xl_index = Excel::ScriptScalarInput(xl_index); const Excel::ScriptSettingsInput_ normalizedindex(xl_index); xl_index = normalizedindex.Get();
        argName = "index (input #2)";
        const double index = Excel::ToDouble(xl_index);
        argName = 0;
		Handle_<StorableLsmcExercisePolicy_> policy;
        BlackScholesLsmcCurvatureResult_Get_BasePolicy(result, index, &policy);
        Excel::Retval_ retval;
        retval.Load(_env, policy);
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

struct XlRegister_BlackScholesLsmcCurvatureResult_Get_BasePolicy_
{
    XlRegister_BlackScholesLsmcCurvatureResult_Get_BasePolicy_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable result");
        argHelp.push_back("Exact zero-based policy ordinal below BasePolicyCount");
        Excel::Register("Base", "xl_BlackScholesLsmcCurvatureResult_Get_BasePolicy", "BLACKSCHOLESLSMCCURVATURERESULT.GET.BASEPOLICY", "Copy BlackScholesLsmcCurvatureResult Get BasePolicy", "QQQ", "result,index", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcCurvatureResult_Get_BasePolicy_ The_BlackScholesLsmcCurvatureResult_Get_BasePolicy_XlRegisterer;
