

extern "C" __declspec(dllexport) OPER_* xl_MarketIvEvaluationSettings_Get_Configuration
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvEvaluationSettings_Get_Configuration");        
        argName = "settings (input #1)";
        const Handle_<StorableMarketIvEvaluationSettings_> settings = Excel::ToHandle<StorableMarketIvEvaluationSettings_>(_env, xl_settings);
        argName = 0;
		Matrix_<Cell_> configuration;
        MarketIvEvaluationSettings_Get_Configuration(settings, &configuration);
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

struct XlRegister_MarketIvEvaluationSettings_Get_Configuration_
{
    XlRegister_MarketIvEvaluationSettings_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("settings");
        Excel::Register("Base", "xl_MarketIvEvaluationSettings_Get_Configuration", "MARKETIVEVALUATIONSETTINGS.GET.CONFIGURATION", "Market IV surface copied diagnostics", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_MarketIvEvaluationSettings_Get_Configuration_ The_MarketIvEvaluationSettings_Get_Configuration_XlRegisterer;
