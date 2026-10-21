

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
        argHelp.push_back("2-col key/value; blank=defaults, compiled=true. RNG/bridge/AAD/smooth/compiled/normal_precision; lsmc_* paths/basis/seeds/risk. lsmc_checkpoint_block_events>0 needs lsmc_training_memory_mb>0 MiB; compiled,1 worker,exact BS only; unset=full storage.");
        Excel::Register("Base", "xl_MonteCarloSettings_New", "MONTECARLOSETTINGS.NEW", "Create immutable Monte Carlo execution settings", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_MonteCarloSettings_New_ The_MonteCarloSettings_New_XlRegisterer;
