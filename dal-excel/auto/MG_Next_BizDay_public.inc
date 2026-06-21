

extern "C" __declspec(dllexport) OPER_* xl_Next_BizDay
    (const OPER_* xl_center, const OPER_* xl_date)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Next_BizDay");        
        argName = "center (input #1)";
        const String_ center = Excel::ToString(xl_center);
        argName = "date (input #2)";
        const Date_ date = Excel::ToDate(xl_date);
        argName = 0;
		Date_ result;
        Next_BizDay(center, date, &result);
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

struct XlRegister_Next_BizDay_
{
    XlRegister_Next_BizDay_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Holiday center name (e.g. \"BEY\" for Beijing, empty string for no holidays)");
        argHelp.push_back("The starting date");
        Excel::Register("Base", "xl_Next_BizDay", "NEXT.BIZDAY", "Find the next business day on or after the given date", "QQQ", "center,date", argHelp, false);
    }
};
static XlRegister_Next_BizDay_ The_Next_BizDay_XlRegisterer;
