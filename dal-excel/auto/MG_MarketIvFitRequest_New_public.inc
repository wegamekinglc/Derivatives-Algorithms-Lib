

extern "C" __declspec(dllexport) OPER_* xl_MarketIvFitRequest_New
    (const OPER_* xl_name, const OPER_* xl_market, const OPER_* xl_quotes, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvFitRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        Excel::ValidateMarketIvRange(xl_quotes, "quotes"); const Excel::ScriptSettingsInput_ normalized_quotes(xl_quotes); xl_quotes = normalized_quotes.Get();
        argName = "market (input #2)";
        const Handle_<StorableMarketIvMarket_> market = Excel::ToHandle<StorableMarketIvMarket_>(_env, xl_market);
        argName = "quotes (input #3)";
        const Matrix_<Cell_> quotes = Excel::ToCellMatrix(xl_quotes, false, true);
        argName = "settings (input #4)";
        const Handle_<StorableMarketIvFitSettings_> settings = Excel::ToHandle<StorableMarketIvFitSettings_>(_env, xl_settings, true);
        argName = 0;
		Handle_<StorableMarketIvFitRequest_> request;
        MarketIvFitRequest_New(name, market, quotes, settings, &request);
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

struct XlRegister_MarketIvFitRequest_New_
{
    XlRegister_MarketIvFitRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("market");
        argHelp.push_back("quotes");
        argHelp.push_back("settings");
        Excel::Register("Base", "xl_MarketIvFitRequest_New", "MARKETIVFITREQUEST.NEW", "Market IV surface owning value", "QQQQQ", "name,market,quotes,[settings]", argHelp, false);
    }
};
static XlRegister_MarketIvFitRequest_New_ The_MarketIvFitRequest_New_XlRegisterer;
