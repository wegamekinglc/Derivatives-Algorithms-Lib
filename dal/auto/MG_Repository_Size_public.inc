

extern "C" __declspec(dllexport) OPER_* xl_Repository_Size
    ()	
{
    Excel::InitializeSessionIfNeeded();
	ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Repository_Size");        
        argName = 0;
		int size;
        Repository_Size(&size);
        Excel::Retval_ retval;
        retval.Load(size);
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

struct XlRegister_Repository_Size_
{
    XlRegister_Repository_Size_()
    {
        Vector_<String_> argHelp;        
        Excel::Register("Bones", "xl_Repository_Size", "REPOSITORY.SIZE", "Number of objects in the repository", "Q!", "", argHelp, false);
    }
};
static XlRegister_Repository_Size_ The_Repository_Size_XlRegisterer;
