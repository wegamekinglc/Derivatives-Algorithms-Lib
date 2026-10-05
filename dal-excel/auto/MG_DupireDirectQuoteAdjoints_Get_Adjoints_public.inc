

extern "C" __declspec(dllexport) OPER_* xl_DupireDirectQuoteAdjoints_Get_Adjoints
    (const OPER_* xl_direct)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireDirectQuoteAdjoints_Get_Adjoints");        
        argName = "direct (input #1)";
        const Handle_<StorableDupireDirectQuoteAdjoints_> direct = Excel::ToHandle<StorableDupireDirectQuoteAdjoints_>(_env, xl_direct);
        argName = 0;
		Matrix_<double> adjoints;
        DupireDirectQuoteAdjoints_Get_Adjoints(direct, &adjoints);
        Excel::Retval_ retval;
        retval.Load(adjoints);
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

struct XlRegister_DupireDirectQuoteAdjoints_Get_Adjoints_
{
    XlRegister_DupireDirectQuoteAdjoints_Get_Adjoints_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Direct quote seeds");
        Excel::Register("Base", "xl_DupireDirectQuoteAdjoints_Get_Adjoints", "DUPIREDIRECTQUOTEADJOINTS.GET.ADJOINTS", "Copy direct quote adjoints", "QQ", "direct", argHelp, false);
    }
};
static XlRegister_DupireDirectQuoteAdjoints_Get_Adjoints_ The_DupireDirectQuoteAdjoints_Get_Adjoints_XlRegisterer;
