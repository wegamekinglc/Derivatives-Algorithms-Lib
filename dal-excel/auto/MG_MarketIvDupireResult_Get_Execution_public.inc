

extern "C" __declspec(dllexport) OPER_* xl_MarketIvDupireResult_Get_Execution
    (const OPER_* xl_source)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvDupireResult_Get_Execution");        
        argName = "source (input #1)";
        const Handle_<StorableMarketIvDupireResult_> source = Excel::ToHandle<StorableMarketIvDupireResult_>(_env, xl_source);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvDupireResult_Get_Execution(source, &table);
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

struct XlRegister_MarketIvDupireResult_Get_Execution_
{
    XlRegister_MarketIvDupireResult_Get_Execution_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("source");
        Excel::Register("Base", "xl_MarketIvDupireResult_Get_Execution", "MARKETIVDUPIRERESULT.GET.EXECUTION", "Market IV surface copied diagnostics", "QQ", "source", argHelp, false);
    }
};
static XlRegister_MarketIvDupireResult_Get_Execution_ The_MarketIvDupireResult_Get_Execution_XlRegisterer;
