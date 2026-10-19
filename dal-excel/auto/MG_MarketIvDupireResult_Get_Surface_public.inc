

extern "C" __declspec(dllexport) OPER_* xl_MarketIvDupireResult_Get_Surface
    (const OPER_* xl_source)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvDupireResult_Get_Surface");        
        argName = "source (input #1)";
        const Handle_<StorableMarketIvDupireResult_> source = Excel::ToHandle<StorableMarketIvDupireResult_>(_env, xl_source);
        argName = 0;
		Handle_<LocalVolSurfaceData_> value;
        MarketIvDupireResult_Get_Surface(source, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_MarketIvDupireResult_Get_Surface_
{
    XlRegister_MarketIvDupireResult_Get_Surface_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("source");
        Excel::Register("Base", "xl_MarketIvDupireResult_Get_Surface", "MARKETIVDUPIRERESULT.GET.SURFACE", "Market IV surface copied diagnostics", "QQ", "source", argHelp, false);
    }
};
static XlRegister_MarketIvDupireResult_Get_Surface_ The_MarketIvDupireResult_Get_Surface_XlRegisterer;
