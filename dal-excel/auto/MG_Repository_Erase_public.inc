

extern "C" __declspec(dllexport) OPER_* xl_Repository_Erase
    (const OPER_* xl_objects)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Repository_Erase");        
        argName = "objects (input #1)";
        const Vector_<Handle_<Storable_>> objects = Excel::ToHandleBaseVector(_env, xl_objects);
        argName = 0;
		int num_erased;
        Repository_Erase(objects, &num_erased);
        Excel::Retval_ retval;
        retval.Load(num_erased);
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

struct XlRegister_Repository_Erase_
{
    XlRegister_Repository_Erase_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The objects to remove(their handles will be invalidated)");
        Excel::Register("Base", "xl_Repository_Erase", "REPOSITORY.ERASE", "Erase objects from repository", "QQ!", "objects", argHelp, false);
    }
};
static XlRegister_Repository_Erase_ The_Repository_Erase_XlRegisterer;
