

extern "C" __declspec(dllexport) OPER_* xl_MarketIvLocalVolModelSettings_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvLocalVolModelSettings_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "InvalidMarketIvLocalVolRequest: /name"); Excel::ValidateMarketIvRange(xl_settings, "settings", true); const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, false, true);
        argName = 0;
		Handle_<StorableMarketIvLocalVolModelSettings_> settingsHandle;
        MarketIvLocalVolModelSettings_New(name, settings, &settingsHandle);
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

struct XlRegister_MarketIvLocalVolModelSettings_New_
{
    XlRegister_MarketIvLocalVolModelSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("settings");
        Excel::Register("Base", "xl_MarketIvLocalVolModelSettings_New", "MARKETIVLOCALVOLMODELSETTINGS.NEW", "Same-source market IV local-volatility model", "QQQ", "name,settings", argHelp, false);
    }
};
static XlRegister_MarketIvLocalVolModelSettings_New_ The_MarketIvLocalVolModelSettings_New_XlRegisterer;
