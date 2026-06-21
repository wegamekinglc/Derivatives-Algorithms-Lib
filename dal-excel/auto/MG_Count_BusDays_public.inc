

extern "C" __declspec(dllexport) OPER_* xl_Count_BusDays
    (const OPER_* xl_center, const OPER_* xl_begin, const OPER_* xl_end)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Count_BusDays");        
        argName = "center (input #1)";
        const String_ center = Excel::ToString(xl_center);
        argName = "begin (input #2)";
        const Date_ begin = Excel::ToDate(xl_begin);
        argName = "end (input #3)";
        const Date_ end = Excel::ToDate(xl_end);
        argName = 0;
		int result;
        Count_BusDays(center, begin, end, &result);
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

struct XlRegister_Count_BusDays_
{
    XlRegister_Count_BusDays_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Holiday center name (e.g. \"BEY\" for Beijing, empty string for no holidays)");
        argHelp.push_back("The start date (inclusive)");
        argHelp.push_back("The end date (inclusive)");
        Excel::Register("Base", "xl_Count_BusDays", "COUNT.BUSDAYS", "Count business days between two dates", "QQQQ", "center,begin,end", argHelp, false);
    }
};
static XlRegister_Count_BusDays_ The_Count_BusDays_XlRegisterer;
