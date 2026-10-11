

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskSettings_Get_Configuration
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskSettings_Get_Configuration");        
        argName = "settings (input #1)";
        const Handle_<StorableEuropeanPdeRiskSettings_> settings = Excel::ToHandle<StorableEuropeanPdeRiskSettings_>(_env, xl_settings);
        argName = 0;
		Matrix_<Cell_> configuration;
        EuropeanPdeRiskSettings_Get_Configuration(settings, &configuration);
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

struct XlRegister_EuropeanPdeRiskSettings_Get_Configuration_
{
    XlRegister_EuropeanPdeRiskSettings_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Immutable settings");
        Excel::Register("Base", "xl_EuropeanPdeRiskSettings_Get_Configuration", "EUROPEANPDERISKSETTINGS.GET.CONFIGURATION", "Copy European PDE settings and optional native budgets", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskSettings_Get_Configuration_ The_EuropeanPdeRiskSettings_Get_Configuration_XlRegisterer;
