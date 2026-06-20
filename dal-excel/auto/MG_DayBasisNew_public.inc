

extern "C" __declspec(dllexport) OPER_* xl_DayBasisNew
    (const OPER_* xl_name)
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("DayBasisNew");
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = 0;
		Handle_<StorableDayBasis_> basis;
        DayBasisNew(name, &basis);
        Excel::Retval_ retval;
        retval.Load(_env, basis);
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

struct XlRegister_DayBasisNew_
{
    XlRegister_DayBasisNew_()
    {
        Vector_<String_> argHelp;
        argHelp.push_back("The day basis name (e.g. \"ACT_365F\", \"ACT_360\", \"30_360\")");
        Excel::Register("Base", "xl_DayBasisNew", "DA.DAY.BASIS.NEW", "Create a day basis handle from a name string", "QQ!", "name", argHelp, false);
    }
};
static XlRegister_DayBasisNew_ The_DayBasisNew_XlRegisterer;
