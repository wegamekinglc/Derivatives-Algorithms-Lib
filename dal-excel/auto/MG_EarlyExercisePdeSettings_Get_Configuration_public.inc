

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeSettings_Get_Configuration
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeSettings_Get_Configuration");        
        argName = "settings (input #1)";
        const Handle_<StorableEarlyExercisePdeSettings_> settings = Excel::ToHandle<StorableEarlyExercisePdeSettings_>(_env, xl_settings);
        argName = 0;
		Matrix_<Cell_> configuration;
        EarlyExercisePdeSettings_Get_Configuration(settings, &configuration);
        Excel::Retval_ retval;
        retval.Load(configuration);
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

struct XlRegister_EarlyExercisePdeSettings_Get_Configuration_
{
    XlRegister_EarlyExercisePdeSettings_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Settings handle");
        Excel::Register("Base", "xl_EarlyExercisePdeSettings_Get_Configuration", "EARLYEXERCISEPDESETTINGS.GET.CONFIGURATION", "Copy all early exercise settings", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeSettings_Get_Configuration_ The_EarlyExercisePdeSettings_Get_Configuration_XlRegisterer;
