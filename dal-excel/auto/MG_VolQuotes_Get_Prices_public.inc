

extern "C" __declspec(dllexport) OPER_* xl_VolQuotes_Get_Prices
    (const OPER_* xl_snapshot, const OPER_* xl_quotes)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("VolQuotes_Get_Prices");        
        argName = "snapshot (input #1)";
        const Handle_<GSRCurveData_> snapshot = Excel::ToHandle<GSRCurveData_>(_env, xl_snapshot);
        argName = "quotes (input #2)";
        const Vector_<Handle_<Storable_>> quotes = Excel::ToHandleBaseVector(_env, xl_quotes);
        argName = 0;
		Matrix_<Cell_> result;
        VolQuotes_Get_Prices(snapshot, quotes, &result);
        Excel::Retval_ retval;
        retval.Load(result);
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

struct XlRegister_VolQuotes_Get_Prices_
{
    XlRegister_VolQuotes_Get_Prices_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Discount and projection curve snapshot");
        argHelp.push_back("Market quote handles");
        Excel::Register("Base", "xl_VolQuotes_Get_Prices", "VOLQUOTES.GET.PRICES", "Convert market volatility quotes on a curve snapshot", "QQQ", "snapshot,quotes", argHelp, false);
    }
};
static XlRegister_VolQuotes_Get_Prices_ The_VolQuotes_Get_Prices_XlRegisterer;
