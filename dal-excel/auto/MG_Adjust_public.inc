

extern "C" __declspec(dllexport) OPER_* xl_Adjust
    (const OPER_* xl_center, const OPER_* xl_date, const OPER_* xl_convention)
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("Adjust");
        argName = "center (input #1)";
        const String_ center = Excel::ToString(xl_center);
        argName = "date (input #2)";
        const Date_ date = Excel::ToDate(xl_date);
        argName = "convention (input #3)";
        const String_ convention = Excel::ToString(xl_convention);
        argName = 0;
		Date_ result;
        Adjust(center, date, convention, &result);
        Excel::Retval_ retval;
        retval.Load(Cell_(result));
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

struct XlRegister_Adjust_
{
    XlRegister_Adjust_()
    {
        Vector_<String_> argHelp;
        argHelp.push_back("Holiday center name (e.g. \"BEY\" for Beijing, empty string for no holidays)");
        argHelp.push_back("The date to adjust");
        argHelp.push_back("Business day convention: \"Unadjusted\", \"Following\", or \"ModifiedFollowing\"");
        Excel::Register("Base", "xl_Adjust", "DA.ADJUST", "Adjust a date according to a business day convention", "QQQQ!", "center,date,convention", argHelp, false);
    }
};
static XlRegister_Adjust_ The_Adjust_XlRegisterer;
