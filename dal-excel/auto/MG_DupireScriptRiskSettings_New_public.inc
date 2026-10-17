

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskSettings_New
    (const OPER_* xl_name, const OPER_* xl_n_paths, const OPER_* xl_valuation, const OPER_* xl_simulation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskSettings_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptRiskSettings_New; name"); argName = "n_paths"; Excel::ValidateRiskRequestPaths(xl_n_paths); xl_n_paths = Excel::ScriptScalarInput(xl_n_paths); xl_valuation = Excel::ScriptScalarInput(xl_valuation); xl_simulation = Excel::ScriptScalarInput(xl_simulation);
        argName = "n_paths (input #2)";
        const double n_paths = Excel::ToDouble(xl_n_paths);
        argName = "valuation (input #3)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation, true);
        argName = "simulation (input #4)";
        const Handle_<StorableMonteCarloSettings_> simulation = Excel::ToHandle<StorableMonteCarloSettings_>(_env, xl_simulation, true);
        argName = 0;
		Handle_<StorableDupireScriptRiskSettings_> settings;
        DupireScriptRiskSettings_New(name, n_paths, valuation, simulation, &settings);
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

struct XlRegister_DupireScriptRiskSettings_New_
{
    XlRegister_DupireScriptRiskSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Positive exactly integral path count");
        argHelp.push_back("Blank uses default valuation settings");
        argHelp.push_back("Blank enables native AAD; explicit false rejects during planning");
        Excel::Register("Base", "xl_DupireScriptRiskSettings_New", "DUPIRESCRIPTRISKSETTINGS.NEW", "Create passive automatic Dupire execution settings", "QQQQQ", "name,n_paths,[valuation],[simulation]", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskSettings_New_ The_DupireScriptRiskSettings_New_XlRegisterer;
