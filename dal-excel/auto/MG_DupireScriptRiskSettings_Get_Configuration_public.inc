

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskSettings_Get_Configuration
    (const OPER_* xl_settings, const OPER_* format)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskSettings_Get_Configuration");        
        argName = "settings (input #1)";
        const Handle_<StorableDupireScriptRiskSettings_> settings = Excel::ToHandle<StorableDupireScriptRiskSettings_>(_env, xl_settings);
        argName = 0;
		double n_paths;
		Handle_<StorableScriptValuationSettings_> valuation;
		Handle_<StorableMonteCarloSettings_> simulation;
        DupireScriptRiskSettings_Get_Configuration(settings, &n_paths, &valuation, &simulation);
        Excel::Retval_ retval;
        retval.Load(n_paths);
        retval.Load(_env, valuation);
        retval.Load(_env, simulation);
        return retval.ToXloper(Excel::ToString(format, true));
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

struct XlRegister_DupireScriptRiskSettings_Get_Configuration_
{
    XlRegister_DupireScriptRiskSettings_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Execution settings");
        Excel::Register("Base", "xl_DupireScriptRiskSettings_Get_Configuration", "DUPIRESCRIPTRISKSETTINGS.GET.CONFIGURATION", "Copy execution count and typed settings", "QQQ", "settings,format", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskSettings_Get_Configuration_ The_DupireScriptRiskSettings_Get_Configuration_XlRegisterer;
