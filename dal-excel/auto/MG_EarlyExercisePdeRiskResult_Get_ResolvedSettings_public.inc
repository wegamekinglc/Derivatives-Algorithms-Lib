

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskResult_Get_ResolvedSettings
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskResult_Get_ResolvedSettings");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskResult_> result = Excel::ToHandle<StorableEarlyExercisePdeRiskResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableEarlyExercisePdeRiskSettings_> settings;
        EarlyExercisePdeRiskResult_Get_ResolvedSettings(result, &settings);
        Excel::Retval_ retval;
        retval.Load(_env, settings);
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

struct XlRegister_EarlyExercisePdeRiskResult_Get_ResolvedSettings_
{
    XlRegister_EarlyExercisePdeRiskResult_Get_ResolvedSettings_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskResult_Get_ResolvedSettings", "EARLYEXERCISEPDERISKRESULT.GET.RESOLVEDSETTINGS", "Copy early exercise risk ResolvedSettings without native solves", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskResult_Get_ResolvedSettings_ The_EarlyExercisePdeRiskResult_Get_ResolvedSettings_XlRegisterer;
