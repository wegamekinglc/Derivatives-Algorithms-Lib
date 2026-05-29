

extern "C" __declspec(dllexport) OPER_* xl_Repository_Find
    (const OPER_* xl_match)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Repository_Find");        
        argName = "match (input #1)";
        const String_ match = Excel::ToString(xl_match);
        argName = 0;
		Vector_<Handle_<Storable_>> objects;
        Repository_Find(match, &objects);
        Excel::Retval_ retval;
        retval.Load(_env, objects);
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

struct XlRegister_Repository_Find_
{
    XlRegister_Repository_Find_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A pattern to match (by search)");
        Excel::Register("Base", "xl_Repository_Find", "REPOSITORY.FIND", "Find existing objects in the repository", "QQ!", "match", argHelp, false);
    }
};
static XlRegister_Repository_Find_ The_Repository_Find_XlRegisterer;
