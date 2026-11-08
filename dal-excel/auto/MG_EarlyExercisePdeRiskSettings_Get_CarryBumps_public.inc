

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskSettings_Get_CarryBumps
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskSettings_Get_CarryBumps");        
        argName = "settings (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskSettings_> settings = Excel::ToHandle<StorableEarlyExercisePdeRiskSettings_>(_env, xl_settings);
        argName = 0;
		Matrix_<Cell_> bumps;
        EarlyExercisePdeRiskSettings_Get_CarryBumps(settings, &bumps);
        Excel::Retval_ retval;
        retval.Load(bumps);
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

struct XlRegister_EarlyExercisePdeRiskSettings_Get_CarryBumps_
{
    XlRegister_EarlyExercisePdeRiskSettings_Get_CarryBumps_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskSettings_Get_CarryBumps", "EARLYEXERCISEPDERISKSETTINGS.GET.CARRYBUMPS", "Copy early exercise risk CarryBumps without native solves", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskSettings_Get_CarryBumps_ The_EarlyExercisePdeRiskSettings_Get_CarryBumps_XlRegisterer;
