

extern "C" __declspec(dllexport) OPER_* xl_PeriodLength_New
    (const OPER_* xl_iso)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PeriodLength_New");        
        argName = "iso (input #1)";
        const String_ iso = Excel::ToString(xl_iso);
        argName = 0;
		Handle_<StorablePeriodLength_> period;
        PeriodLength_New(iso, &period);
        Excel::Retval_ retval;
        retval.Load(_env, period);
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

struct XlRegister_PeriodLength_New_
{
    XlRegister_PeriodLength_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The ISO period string (e.g. \"3M\", \"6M\", \"12M\")");
        Excel::Register("Base", "xl_PeriodLength_New", "PERIODLENGTH.NEW", "Create a period length handle from an ISO string", "QQ", "iso", argHelp, false);
    }
};
static XlRegister_PeriodLength_New_ The_PeriodLength_New_XlRegisterer;
