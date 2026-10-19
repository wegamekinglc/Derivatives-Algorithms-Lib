

extern "C" __declspec(dllexport) OPER_* xl_MarketIvQuery_New
    (const OPER_* xl_name, const OPER_* xl_strike, const OPER_* xl_time)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvQuery_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "strike (input #2)";
        const double strike = Excel::ToDouble(xl_strike);
        argName = "time (input #3)";
        const double time = Excel::ToDouble(xl_time);
        Excel::ValidateMarketIvNumber(xl_strike, "strike"); const Excel::ScriptSettingsInput_ normalized_strike(xl_strike); xl_strike = normalized_strike.Get(); Excel::ValidateMarketIvNumber(xl_time, "time"); const Excel::ScriptSettingsInput_ normalized_time(xl_time); xl_time = normalized_time.Get();
        argName = 0;
		Handle_<StorableMarketIvQuery_> query;
        MarketIvQuery_New(name, strike, time, &query);
        Excel::Retval_ retval;
        retval.Load(_env, query);
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

struct XlRegister_MarketIvQuery_New_
{
    XlRegister_MarketIvQuery_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("strike");
        argHelp.push_back("time");
        Excel::Register("Base", "xl_MarketIvQuery_New", "MARKETIVQUERY.NEW", "Market IV surface owning value", "QQQQ", "name,strike,time", argHelp, false);
    }
};
static XlRegister_MarketIvQuery_New_ The_MarketIvQuery_New_XlRegisterer;
