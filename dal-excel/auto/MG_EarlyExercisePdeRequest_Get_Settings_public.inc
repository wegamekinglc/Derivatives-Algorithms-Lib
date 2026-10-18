

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRequest_Get_Settings
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRequest_Get_Settings");        
        argName = "request (input #1)";
        const Handle_<StorableEarlyExercisePdeRequest_> request = Excel::ToHandle<StorableEarlyExercisePdeRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableEarlyExercisePdeSettings_> settings;
        EarlyExercisePdeRequest_Get_Settings(request, &settings);
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

struct XlRegister_EarlyExercisePdeRequest_Get_Settings_
{
    XlRegister_EarlyExercisePdeRequest_Get_Settings_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Request handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRequest_Get_Settings", "EARLYEXERCISEPDEREQUEST.GET.SETTINGS", "Copy request settings", "QQ", "request", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRequest_Get_Settings_ The_EarlyExercisePdeRequest_Get_Settings_XlRegisterer;
