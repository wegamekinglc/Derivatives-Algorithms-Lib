

extern "C" __declspec(dllexport) OPER_* xl_PeriodLengthNew
    (const OPER_* xl_iso)
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("PeriodLengthNew");
        argName = "iso (input #1)";
        const String_ iso = Excel::ToString(xl_iso);
        argName = 0;
		Handle_<StorablePeriodLength_> period;
        PeriodLengthNew(iso, &period);
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

struct XlRegister_PeriodLengthNew_
{
    XlRegister_PeriodLengthNew_()
    {
        Vector_<String_> argHelp;
        argHelp.push_back("The ISO period string (e.g. \"3M\", \"6M\", \"12M\")");
        Excel::Register("Base", "xl_PeriodLengthNew", "DA.PERIOD.LENGTH.NEW", "Create a period length handle from an ISO string", "QQ!", "iso", argHelp, false);
    }
};
static XlRegister_PeriodLengthNew_ The_PeriodLengthNew_XlRegisterer;
