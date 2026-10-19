

extern "C" __declspec(dllexport) OPER_* xl_MarketIvDupireResult_Get_Failure
    (const OPER_* xl_source)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvDupireResult_Get_Failure");        
        argName = "source (input #1)";
        const Handle_<StorableMarketIvDupireResult_> source = Excel::ToHandle<StorableMarketIvDupireResult_>(_env, xl_source);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvDupireResult_Get_Failure(source, &table);
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

struct XlRegister_MarketIvDupireResult_Get_Failure_
{
    XlRegister_MarketIvDupireResult_Get_Failure_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("source");
        Excel::Register("Base", "xl_MarketIvDupireResult_Get_Failure", "MARKETIVDUPIRERESULT.GET.FAILURE", "Market IV surface copied diagnostics", "QQ", "source", argHelp, false);
    }
};
static XlRegister_MarketIvDupireResult_Get_Failure_ The_MarketIvDupireResult_Get_Failure_XlRegisterer;
