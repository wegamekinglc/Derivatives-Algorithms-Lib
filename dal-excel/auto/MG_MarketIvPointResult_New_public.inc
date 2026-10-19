

extern "C" __declspec(dllexport) OPER_* xl_MarketIvPointResult_New
    (const OPER_* xl_name, const OPER_* xl_snapshot, const OPER_* xl_query, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvPointResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "snapshot (input #2)";
        const Handle_<StorableMarketIvCalibrationSnapshot_> snapshot = Excel::ToHandle<StorableMarketIvCalibrationSnapshot_>(_env, xl_snapshot);
        argName = "query (input #3)";
        const Handle_<StorableMarketIvQuery_> query = Excel::ToHandle<StorableMarketIvQuery_>(_env, xl_query);
        argName = "settings (input #4)";
        const Handle_<StorableMarketIvEvaluationSettings_> settings = Excel::ToHandle<StorableMarketIvEvaluationSettings_>(_env, xl_settings, true);
        argName = 0;
		Handle_<StorableMarketIvPointResult_> result;
        MarketIvPointResult_New(name, snapshot, query, settings, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_MarketIvPointResult_New_
{
    XlRegister_MarketIvPointResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("snapshot");
        argHelp.push_back("query");
        argHelp.push_back("settings");
        Excel::Register("Base", "xl_MarketIvPointResult_New", "MARKETIVPOINTRESULT.NEW", "Market IV surface owning value", "QQQQQ", "name,snapshot,query,[settings]", argHelp, false);
    }
};
static XlRegister_MarketIvPointResult_New_ The_MarketIvPointResult_New_XlRegisterer;
