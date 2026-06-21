

extern "C" __declspec(dllexport) OPER_* xl_CountBusDays
    (const OPER_* xl_center, const OPER_* xl_begin, const OPER_* xl_end)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CountBusDays");        
        argName = "center (input #1)";
        const String_ center = Excel::ToString(xl_center);
        argName = "begin (input #2)";
        const Date_ begin = Excel::ToDate(xl_begin);
        argName = "end (input #3)";
        const Date_ end = Excel::ToDate(xl_end);
        argName = 0;
		int result;
        CountBusDays(center, begin, end, &result);
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

struct XlRegister_CountBusDays_
{
    XlRegister_CountBusDays_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Holiday center name (e.g. \"BEY\" for Beijing, empty string for no holidays)");
        argHelp.push_back("The start date (inclusive)");
        argHelp.push_back("The end date (inclusive)");
        Excel::Register("Base", "xl_CountBusDays", "DA.COUNTBUSDAYS", "Count business days between two dates", "QQQQ", "center,begin,end", argHelp, false);
    }
};
static XlRegister_CountBusDays_ The_CountBusDays_XlRegisterer;
