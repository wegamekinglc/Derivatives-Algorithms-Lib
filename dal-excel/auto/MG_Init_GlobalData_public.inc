

extern "C" __declspec(dllexport) OPER_* xl_Init_GlobalData
    (const OPER_* xl_nThreads)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Init_GlobalData");        
        argName = "nThreads (input #1)";
        const int nThreads = Excel::ToInt(xl_nThreads);
        argName = 0;
		bool ok;
        Init_GlobalData(nThreads, &ok);
        Excel::Retval_ retval;
        retval.Load(ok);
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

struct XlRegister_Init_GlobalData_
{
    XlRegister_Init_GlobalData_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("number of threads (0 = auto-detect, default)");
        Excel::Register("Base", "xl_Init_GlobalData", "INIT.GLOBALDATA", "Initialize the DAL global environment (thread pool, repository, etc.)", "QQ", "nThreads", argHelp, false);
    }
};
static XlRegister_Init_GlobalData_ The_Init_GlobalData_XlRegisterer;
