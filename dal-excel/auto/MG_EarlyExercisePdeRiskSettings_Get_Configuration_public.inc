

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskSettings_Get_Configuration
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskSettings_Get_Configuration");        
        argName = "settings (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskSettings_> settings = Excel::ToHandle_orEarlyExerciseRisk<StorableEarlyExercisePdeRiskSettings_>(_env, xl_settings, "settings");
        argName = 0;
		Matrix_<Cell_> configuration;
        EarlyExercisePdeRiskSettings_Get_Configuration(settings, &configuration);
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

struct XlRegister_EarlyExercisePdeRiskSettings_Get_Configuration_
{
    XlRegister_EarlyExercisePdeRiskSettings_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskSettings_Get_Configuration", "EARLYEXERCISEPDERISKSETTINGS.GET.CONFIGURATION", "Copy early exercise risk Configuration without native solves", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskSettings_Get_Configuration_ The_EarlyExercisePdeRiskSettings_Get_Configuration_XlRegisterer;
