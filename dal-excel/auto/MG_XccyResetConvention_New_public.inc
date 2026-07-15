

extern "C" __declspec(dllexport) OPER_* xl_XccyResetConvention_New
    (const OPER_* xl_fixingLag, const OPER_* xl_fixingHolidays, const OPER_* xl_fixingConvention, const OPER_* xl_fixingHour, const OPER_* xl_fixingMinute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyResetConvention_New");        
        argName = "fixingLag (input #1)";
        const int fixingLag = Excel::ToInt(xl_fixingLag);
        argName = "fixingHolidays (input #2)";
        const String_ fixingHolidays = Excel::ToString(xl_fixingHolidays);
        argName = "fixingConvention (input #3)";
        const String_ fixingConvention = Excel::ToString(xl_fixingConvention);
        argName = "fixingHour (input #4)";
        const int fixingHour = Excel::ToInt(xl_fixingHour);
        argName = "fixingMinute (input #5)";
        const int fixingMinute = Excel::ToInt(xl_fixingMinute);
        argName = 0;
		Handle_<StorableFxResetConvention_> resetConvention;
        XccyResetConvention_New(fixingLag, fixingHolidays, fixingConvention, fixingHour, fixingMinute, &resetConvention);
        Excel::Retval_ retval;
        retval.Load(_env, resetConvention);
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

struct XlRegister_XccyResetConvention_New_
{
    XlRegister_XccyResetConvention_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Number of business days before the reset date used for the FX fixing");
        argHelp.push_back("Holiday center used to adjust FX fixing dates (empty string means no holidays)");
        argHelp.push_back("Business-day convention used to adjust FX fixing dates");
        argHelp.push_back("FX fixing hour in the range 0 to 23");
        argHelp.push_back("FX fixing minute in the range 0 to 59");
        Excel::Register("Base", "xl_XccyResetConvention_New", "XCCYRESETCONVENTION.NEW", "Create an FX reset convention handle for resettable cross-currency swaps", "QQQQQQ", "fixingLag,fixingHolidays,fixingConvention,fixingHour,fixingMinute", argHelp, false);
    }
};
static XlRegister_XccyResetConvention_New_ The_XccyResetConvention_New_XlRegisterer;
