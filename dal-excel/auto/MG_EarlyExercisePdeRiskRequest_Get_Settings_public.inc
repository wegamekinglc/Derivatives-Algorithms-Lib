

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskRequest_Get_Settings
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskRequest_Get_Settings");        
        argName = "request (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskRequest_> request = Excel::ToHandle<StorableEarlyExercisePdeRiskRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableEarlyExercisePdeRiskSettings_> settings;
        EarlyExercisePdeRiskRequest_Get_Settings(request, &settings);
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

struct XlRegister_EarlyExercisePdeRiskRequest_Get_Settings_
{
    XlRegister_EarlyExercisePdeRiskRequest_Get_Settings_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskRequest_Get_Settings", "EARLYEXERCISEPDERISKREQUEST.GET.SETTINGS", "Copy early exercise risk Settings without native solves", "QQ", "request", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskRequest_Get_Settings_ The_EarlyExercisePdeRiskRequest_Get_Settings_XlRegisterer;
