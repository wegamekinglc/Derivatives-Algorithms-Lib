

extern "C" __declspec(dllexport) OPER_* xl_InitGlobalData
    (const OPER_* xl_nThreads)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("InitGlobalData");        
        argName = "nThreads (input #1)";
        const int nThreads = Excel::ToInt(xl_nThreads);
        argName = 0;
		bool ok;
        InitGlobalData(nThreads, &ok);
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

struct XlRegister_InitGlobalData_
{
    XlRegister_InitGlobalData_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("number of threads (0 = auto-detect, default)");
        Excel::Register("Base", "xl_InitGlobalData", "DA.INITGLOBALDATA", "Initialize the DAL global environment (thread pool, repository, etc.)", "QQ", "nThreads", argHelp, false);
    }
};
static XlRegister_InitGlobalData_ The_InitGlobalData_XlRegisterer;
