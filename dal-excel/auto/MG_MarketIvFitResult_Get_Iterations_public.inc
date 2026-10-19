

extern "C" __declspec(dllexport) OPER_* xl_MarketIvFitResult_Get_Iterations
    (const OPER_* xl_source)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvFitResult_Get_Iterations");        
        argName = "source (input #1)";
        const Handle_<StorableMarketIvFitResult_> source = Excel::ToHandle<StorableMarketIvFitResult_>(_env, xl_source);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvFitResult_Get_Iterations(source, &table);
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

struct XlRegister_MarketIvFitResult_Get_Iterations_
{
    XlRegister_MarketIvFitResult_Get_Iterations_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("source");
        Excel::Register("Base", "xl_MarketIvFitResult_Get_Iterations", "MARKETIVFITRESULT.GET.ITERATIONS", "Market IV surface copied diagnostics", "QQ", "source", argHelp, false);
    }
};
static XlRegister_MarketIvFitResult_Get_Iterations_ The_MarketIvFitResult_Get_Iterations_XlRegisterer;
