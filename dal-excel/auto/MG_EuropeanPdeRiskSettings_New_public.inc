

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskSettings_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskSettings_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EuropeanPdeRiskSettings_New; name"); const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get(); argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "EuropeanPdeRiskSettings_New");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableEuropeanPdeRiskSettings_> configuration;
        EuropeanPdeRiskSettings_New(name, settings, &configuration);
        Excel::Retval_ retval;
        retval.Load(_env, configuration);
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

struct XlRegister_EuropeanPdeRiskSettings_New_
{
    XlRegister_EuropeanPdeRiskSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Two key/value columns; blank defaults; budgets exclude worksheet cells and labels");
        Excel::Register("Base", "xl_EuropeanPdeRiskSettings_New", "EUROPEANPDERISKSETTINGS.NEW", "Create checked immutable European PDE settings", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskSettings_New_ The_EuropeanPdeRiskSettings_New_XlRegisterer;
