

extern "C" __declspec(dllexport) OPER_* xl_MarketIvDupireSettings_Get_Configuration
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvDupireSettings_Get_Configuration");        
        argName = "settings (input #1)";
        const Handle_<StorableMarketIvDupireSettings_> settings = Excel::ToHandle<StorableMarketIvDupireSettings_>(_env, xl_settings);
        argName = 0;
		Matrix_<Cell_> configuration;
        MarketIvDupireSettings_Get_Configuration(settings, &configuration);
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

struct XlRegister_MarketIvDupireSettings_Get_Configuration_
{
    XlRegister_MarketIvDupireSettings_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("settings");
        Excel::Register("Base", "xl_MarketIvDupireSettings_Get_Configuration", "MARKETIVDUPIRESETTINGS.GET.CONFIGURATION", "Market IV surface copied diagnostics", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_MarketIvDupireSettings_Get_Configuration_ The_MarketIvDupireSettings_Get_Configuration_XlRegisterer;
