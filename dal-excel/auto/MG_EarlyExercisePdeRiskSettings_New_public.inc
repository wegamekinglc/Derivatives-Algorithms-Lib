

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskSettings_New
    (const OPER_* xl_name, const OPER_* xl_settings, const OPER_* xl_carry_bumps)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskSettings_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateEarlyExercisePdeRiskName(xl_name); const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get(); argName = "settings"; Excel::ValidateEarlyExercisePdeRiskRange(xl_settings, "settings"); const Excel::ScriptSettingsInput_ normalized_bumps(xl_carry_bumps); xl_carry_bumps = normalized_bumps.Get(); argName = "carry_bumps"; Excel::ValidateEarlyExercisePdeRiskRange(xl_carry_bumps, "carry_bumps");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = "carry_bumps (input #3)";
        const Matrix_<Cell_> carry_bumps = Excel::ToCellMatrix(xl_carry_bumps, true, true);
        argName = 0;
		Handle_<StorableEarlyExercisePdeRiskSettings_> settingsHandle;
        EarlyExercisePdeRiskSettings_New(name, settings, carry_bumps, &settingsHandle);
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

struct XlRegister_EarlyExercisePdeRiskSettings_New_
{
    XlRegister_EarlyExercisePdeRiskSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Headerless canonical scalar key/value rows; blank uses defaults");
        argHelp.push_back("Headerless rate/yield bump columns; each blank column defaults independently");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskSettings_New", "EARLYEXERCISEPDERISKSETTINGS.NEW", "Create owning early exercise risk settings and cumulative budgets", "QQQQ", "name,[settings],[carry_bumps]", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskSettings_New_ The_EarlyExercisePdeRiskSettings_New_XlRegisterer;
