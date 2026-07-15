

extern "C" __declspec(dllexport) OPER_* xl_MarketFixingSnapshot_New
    (const OPER_* xl_indexNames, const OPER_* xl_fixingTimes, const OPER_* xl_values)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketFixingSnapshot_New");        
        argName = "indexNames (input #1)";
        const Vector_<String_> indexNames = Excel::ToStringVector(xl_indexNames);
        argName = "fixingTimes (input #2)";
        const Vector_<Cell_> fixingTimes = Excel::ToCellVector(xl_fixingTimes);
        argName = "values (input #3)";
        const Vector_<double> values = Excel::ToDoubleVector(xl_values);
        argName = 0;
		Handle_<StorableMarketFixingSnapshot_> snapshot;
        MarketFixingSnapshot_New(indexNames, fixingTimes, values, &snapshot);
        Excel::Retval_ retval;
        retval.Load(_env, snapshot);
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

struct XlRegister_MarketFixingSnapshot_New_
{
    XlRegister_MarketFixingSnapshot_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Index names for the fixing observations");
        argHelp.push_back("Fixing timestamps as Excel date-time cells");
        argHelp.push_back("Positive finite fixing values");
        Excel::Register("Base", "xl_MarketFixingSnapshot_New", "MARKETFIXINGSNAPSHOT.NEW", "Create an immutable market fixing snapshot from parallel input arrays", "QQQQ", "indexNames,fixingTimes,values", argHelp, false);
    }
};
static XlRegister_MarketFixingSnapshot_New_ The_MarketFixingSnapshot_New_XlRegisterer;
