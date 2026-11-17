

extern "C" __declspec(dllexport) OPER_* xl_MarketIvLocalVolModelData_New
    (const OPER_* xl_name, const OPER_* xl_grid, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvLocalVolModelData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "InvalidMarketIvLocalVolRequest: /name");
        argName = "grid (input #2)";
        const Handle_<StorableMarketIvDupireResult_> grid = Excel::ToHandle<StorableMarketIvDupireResult_>(_env, xl_grid);
        argName = "settings (input #3)";
        const Handle_<StorableMarketIvLocalVolModelSettings_> settings = Excel::ToHandle<StorableMarketIvLocalVolModelSettings_>(_env, xl_settings);
        argName = 0;
		Handle_<MarketIvLocalVolModelData_> model;
        MarketIvLocalVolModelData_New(name, grid, settings, &model);
        Excel::Retval_ retval;
        retval.Load(_env, model);
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

struct XlRegister_MarketIvLocalVolModelData_New_
{
    XlRegister_MarketIvLocalVolModelData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("grid");
        argHelp.push_back("settings");
        Excel::Register("Base", "xl_MarketIvLocalVolModelData_New", "MARKETIVLOCALVOLMODELDATA.NEW", "Same-source market IV local-volatility model", "QQQQ", "name,grid,settings", argHelp, false);
    }
};
static XlRegister_MarketIvLocalVolModelData_New_ The_MarketIvLocalVolModelData_New_XlRegisterer;
