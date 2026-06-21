

extern "C" __declspec(dllexport) OPER_* xl_NextBus
    (const OPER_* xl_center, const OPER_* xl_date)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("NextBus");        
        argName = "center (input #1)";
        const String_ center = Excel::ToString(xl_center);
        argName = "date (input #2)";
        const Date_ date = Excel::ToDate(xl_date);
        argName = 0;
		Date_ result;
        NextBus(center, date, &result);
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

struct XlRegister_NextBus_
{
    XlRegister_NextBus_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Holiday center name (e.g. \"BEY\" for Beijing, empty string for no holidays)");
        argHelp.push_back("The starting date");
        Excel::Register("Base", "xl_NextBus", "DA.NEXTBUS", "Find the next business day on or after the given date", "QQQ", "center,date", argHelp, false);
    }
};
static XlRegister_NextBus_ The_NextBus_XlRegisterer;
