

extern "C" __declspec(dllexport) OPER_* xl_MarketIvPointResult_Get_Values
    (const OPER_* xl_source)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvPointResult_Get_Values");        
        argName = "source (input #1)";
        const Handle_<StorableMarketIvPointResult_> source = Excel::ToHandle<StorableMarketIvPointResult_>(_env, xl_source);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvPointResult_Get_Values(source, &table);
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

struct XlRegister_MarketIvPointResult_Get_Values_
{
    XlRegister_MarketIvPointResult_Get_Values_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("source");
        Excel::Register("Base", "xl_MarketIvPointResult_Get_Values", "MARKETIVPOINTRESULT.GET.VALUES", "Market IV surface copied diagnostics", "QQ", "source", argHelp, false);
    }
};
static XlRegister_MarketIvPointResult_Get_Values_ The_MarketIvPointResult_Get_Values_XlRegisterer;
