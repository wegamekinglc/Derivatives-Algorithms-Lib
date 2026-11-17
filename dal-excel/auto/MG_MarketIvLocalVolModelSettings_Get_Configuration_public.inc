

extern "C" __declspec(dllexport) OPER_* xl_MarketIvLocalVolModelSettings_Get_Configuration
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvLocalVolModelSettings_Get_Configuration");        
        argName = "settings (input #1)";
        const Handle_<StorableMarketIvLocalVolModelSettings_> settings = Excel::ToHandle<StorableMarketIvLocalVolModelSettings_>(_env, xl_settings);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvLocalVolModelSettings_Get_Configuration(settings, &table);
        Excel::Retval_ retval;
        retval.Load(table);
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

struct XlRegister_MarketIvLocalVolModelSettings_Get_Configuration_
{
    XlRegister_MarketIvLocalVolModelSettings_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("settings");
        Excel::Register("Base", "xl_MarketIvLocalVolModelSettings_Get_Configuration", "MARKETIVLOCALVOLMODELSETTINGS.GET.CONFIGURATION", "Same-source market IV local-volatility model", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_MarketIvLocalVolModelSettings_Get_Configuration_ The_MarketIvLocalVolModelSettings_Get_Configuration_XlRegisterer;
