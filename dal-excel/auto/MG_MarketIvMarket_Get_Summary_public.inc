

extern "C" __declspec(dllexport) OPER_* xl_MarketIvMarket_Get_Summary
    (const OPER_* xl_source)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvMarket_Get_Summary");        
        argName = "source (input #1)";
        const Handle_<StorableMarketIvMarket_> source = Excel::ToHandle<StorableMarketIvMarket_>(_env, xl_source);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvMarket_Get_Summary(source, &table);
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

struct XlRegister_MarketIvMarket_Get_Summary_
{
    XlRegister_MarketIvMarket_Get_Summary_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("source");
        Excel::Register("Base", "xl_MarketIvMarket_Get_Summary", "MARKETIVMARKET.GET.SUMMARY", "Market IV surface copied diagnostics", "QQ", "source", argHelp, false);
    }
};
static XlRegister_MarketIvMarket_Get_Summary_ The_MarketIvMarket_Get_Summary_XlRegisterer;
