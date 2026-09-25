

extern "C" __declspec(dllexport) OPER_* xl_MonteCarloSettings_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MonteCarloSettings_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get(); argName = "settings (input #2)"; Excel::ValidateScriptSettingsRange(xl_settings, "MonteCarloSettings_New", "settings");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableMonteCarloSettings_> simulation;
        MonteCarloSettings_New(name, settings, &simulation);
        Excel::Retval_ retval;
        retval.Load(_env, simulation);
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

struct XlRegister_MonteCarloSettings_New_
{
    XlRegister_MonteCarloSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Two columns: method, use_bb, enable_aad, smooth, compiled, lsmc_basis_degree, lsmc_training_paths, lsmc_validation_paths, lsmc_rqmc_replicates, lsmc_training_seed, lsmc_pricing_seed, lsmc_policy_risk_mode, lsmc_policy_bump_relative.");
        Excel::Register("Base", "xl_MonteCarloSettings_New", "MONTECARLOSETTINGS.NEW", "Create immutable Monte Carlo execution settings", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_MonteCarloSettings_New_ The_MonteCarloSettings_New_XlRegisterer;
