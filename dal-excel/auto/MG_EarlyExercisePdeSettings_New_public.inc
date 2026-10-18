

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeSettings_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeSettings_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EarlyExercisePdeSettings_New; name"); const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get(); argName = "settings"; Excel::ValidateEarlyExercisePdeSettingsInput(xl_settings, "EarlyExercisePdeSettings_New");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableEarlyExercisePdeSettings_> settingsHandle;
        EarlyExercisePdeSettings_New(name, settings, &settingsHandle);
        Excel::Retval_ retval;
        retval.Load(_env, settingsHandle);
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

struct XlRegister_EarlyExercisePdeSettings_New_
{
    XlRegister_EarlyExercisePdeSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Two key/value columns; blank uses defaults");
        Excel::Register("Base", "xl_EarlyExercisePdeSettings_New", "EARLYEXERCISEPDESETTINGS.NEW", "Create immutable early exercise settings with native budgets", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeSettings_New_ The_EarlyExercisePdeSettings_New_XlRegisterer;
