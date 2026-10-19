

extern "C" __declspec(dllexport) OPER_* xl_MarketIvDupireRequest_New
    (const OPER_* xl_name, const OPER_* xl_spots, const OPER_* xl_times, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvDupireRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "spots (input #2)";
        const Matrix_<Cell_> spots = Excel::ToCellMatrix(xl_spots, false, true);
        argName = "times (input #3)";
        const Matrix_<Cell_> times = Excel::ToCellMatrix(xl_times, false, true);
        Excel::ValidateMarketIvRange(xl_spots, "spots"); const Excel::ScriptSettingsInput_ normalized_spots(xl_spots); xl_spots = normalized_spots.Get(); Excel::ValidateMarketIvRange(xl_times, "times"); const Excel::ScriptSettingsInput_ normalized_times(xl_times); xl_times = normalized_times.Get();
        argName = "settings (input #4)";
        const Handle_<StorableMarketIvDupireSettings_> settings = Excel::ToHandle<StorableMarketIvDupireSettings_>(_env, xl_settings, true);
        argName = 0;
		Handle_<StorableMarketIvDupireRequest_> request;
        MarketIvDupireRequest_New(name, spots, times, settings, &request);
        Excel::Retval_ retval;
        retval.Load(_env, request);
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

struct XlRegister_MarketIvDupireRequest_New_
{
    XlRegister_MarketIvDupireRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("spots");
        argHelp.push_back("times");
        argHelp.push_back("settings");
        Excel::Register("Base", "xl_MarketIvDupireRequest_New", "MARKETIVDUPIREREQUEST.NEW", "Market IV surface owning value", "QQQQQ", "name,spots,times,[settings]", argHelp, false);
    }
};
static XlRegister_MarketIvDupireRequest_New_ The_MarketIvDupireRequest_New_XlRegisterer;
